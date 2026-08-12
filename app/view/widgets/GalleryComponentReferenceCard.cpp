#include "GalleryComponentReferenceCard.h"

#include <QFontDatabase>
#include <QGridLayout>
#include <QSizePolicy>

#include "components/textfields/Label.h"
#include "design/CornerRadius.h"
#include "design/Typography.h"
#include "GalleryLanguageSelector.h"
#include "view/support/GalleryStyleSupport.h"

namespace fluent::gallery {

GalleryComponentReferenceCard::GalleryComponentReferenceCard(
    const GalleryComponentReference& reference,
    QWidget* parent)
    : GalleryComponentReferenceCard(reference, false, parent)
{
}

GalleryComponentReferenceCard::GalleryComponentReferenceCard(
    const GalleryComponentReference& reference,
    bool showLanguageSelector,
    QWidget* parent)
    : QFrame(parent)
    , m_reference(reference)
{
    setObjectName(QStringLiteral("galleryComponentReferenceCard"));
    setFrameShape(QFrame::NoFrame);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    auto* layout = new QGridLayout(this);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setHorizontalSpacing(20);
    layout->setVerticalSpacing(8);
    layout->setColumnStretch(1, 1);

    int row = 0;
    if (showLanguageSelector && m_reference.hasPythonReference()) {
        m_languageSelector = new GalleryLanguageSelector(this);
        m_languageSelector->setObjectName(
            QStringLiteral("galleryComponentReferenceLanguageSelector"));
        layout->addWidget(m_languageSelector, row++, 0, 1, 2, Qt::AlignLeft);
        connect(m_languageSelector,
                &GalleryLanguageSelector::languageChanged,
                this,
                &GalleryComponentReferenceCard::setCodeLanguage);
    }

    addRow(layout, row++,
           QString(),
           QString(),
           QString(),
           true);
    addRow(layout, row++,
           QString(),
           QString(),
           QString(),
           true);
    addRow(layout, row,
           QString(),
           QString(),
           QString(),
           true);

    updateReferenceRows();
    applyPalette();
}

void GalleryComponentReferenceCard::setCodeLanguage(
    GalleryCodeLanguage language)
{
    if (language == GalleryCodeLanguage::Python
        && (!m_languageSelector || !m_reference.hasPythonReference())) {
        return;
    }
    if (m_codeLanguage == language)
        return;

    m_codeLanguage = language;
    if (m_languageSelector)
        m_languageSelector->setLanguage(language);
    updateReferenceRows();
    emit codeLanguageChanged(language);
}

void GalleryComponentReferenceCard::onThemeUpdated()
{
    for (fluent::textfields::Label* label : m_nameLabels) {
        if (label)
            label->onThemeUpdated();
    }
    for (fluent::textfields::Label* label : m_valueLabels) {
        if (label)
            label->onThemeUpdated();
    }
    if (m_languageSelector)
        m_languageSelector->onThemeUpdated();
    applyPalette();
}

void GalleryComponentReferenceCard::addRow(QGridLayout* layout,
                                            int row,
                                            const QString& name,
                                            const QString& value,
                                            const QString& valueObjectName,
                                            bool codeValue)
{
    auto* nameLabel = new fluent::textfields::Label(name, this);
    nameLabel->setObjectName(QStringLiteral("galleryComponentReferenceKey"));
    nameLabel->setFluentTypography(Typography::FontRole::Body);
    nameLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);

    auto* valueLabel = new fluent::textfields::Label(value, this);
    valueLabel->setObjectName(valueObjectName);
    valueLabel->setFluentTypography(Typography::FontRole::BodyStrong);
    valueLabel->setWordWrap(true);
    valueLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    valueLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    if (codeValue) {
        QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
        font.setPixelSize(themeFont(Typography::FontRole::Body).size);
        valueLabel->setFont(font);
    }

    layout->addWidget(nameLabel, row, 0, Qt::AlignTop);
    layout->addWidget(valueLabel, row, 1);
    m_nameLabels.append(nameLabel);
    m_valueLabels.append(valueLabel);
}

void GalleryComponentReferenceCard::updateReferenceRows()
{
    if (m_nameLabels.size() != 3 || m_valueLabels.size() != 3)
        return;

    QStringList names;
    QStringList values;
    QStringList objectNames;
    if (m_codeLanguage == GalleryCodeLanguage::Python) {
        names = QStringList{QStringLiteral("Install"),
                            QStringLiteral("Import"),
                            QStringLiteral("Type")};
        values = QStringList{m_reference.pythonInstall,
                             m_reference.pythonImport,
                             m_reference.pythonType};
        objectNames = QStringList{
            QStringLiteral("galleryComponentReferencePythonInstall"),
            QStringLiteral("galleryComponentReferencePythonImport"),
            QStringLiteral("galleryComponentReferencePythonType")};
    } else {
        names = QStringList{QStringLiteral("Header"),
                            QStringLiteral("Type"),
                            QStringLiteral("CMake target")};
        values = QStringList{m_reference.header,
                             m_reference.qualifiedType,
                             m_reference.cmakeTarget};
        objectNames = QStringList{
            QStringLiteral("galleryComponentReferenceHeader"),
            QStringLiteral("galleryComponentReferenceType"),
            QStringLiteral("galleryComponentReferenceCMakeTarget")};
    }

    for (int i = 0; i < 3; ++i) {
        m_nameLabels.at(i)->setText(names.at(i));
        m_valueLabels.at(i)->setText(values.at(i));
        m_valueLabels.at(i)->setObjectName(objectNames.at(i));
    }
}

void GalleryComponentReferenceCard::applyPalette()
{
    const Colors colors = themeColors();
    setStyleSheet(QStringLiteral(
                      "#galleryComponentReferenceCard { background: %1; border: 1px solid %2; border-radius: %3px; }")
                      .arg(cssColor(colors.bgLayer),
                           cssColor(colors.strokeCard))
                      .arg(::CornerRadius::Overlay));

    for (fluent::textfields::Label* label : m_nameLabels) {
        if (label) {
            label->setStyleSheet(QStringLiteral("color: %1; background: transparent;")
                                     .arg(cssColor(colors.textSecondary)));
        }
    }
    for (fluent::textfields::Label* label : m_valueLabels) {
        if (label) {
            label->setStyleSheet(QStringLiteral("color: %1; background: transparent;")
                                     .arg(cssColor(colors.textPrimary)));
        }
    }
}

} // namespace fluent::gallery

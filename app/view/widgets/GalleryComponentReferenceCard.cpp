#include "GalleryComponentReferenceCard.h"

#include <QFontDatabase>
#include <QGridLayout>
#include <QSizePolicy>

#include "components/textfields/Label.h"
#include "design/CornerRadius.h"
#include "design/Typography.h"
#include "view/support/GalleryStyleSupport.h"

namespace fluent::gallery {

GalleryComponentReferenceCard::GalleryComponentReferenceCard(
    const GalleryComponentReference& reference,
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

    addRow(layout, 0,
           QStringLiteral("Header"),
           reference.header,
           QStringLiteral("galleryComponentReferenceHeader"),
           true);
    addRow(layout, 1,
           QStringLiteral("Type"),
           reference.qualifiedType,
           QStringLiteral("galleryComponentReferenceType"),
           true);
    addRow(layout, 2,
           QStringLiteral("CMake target"),
           reference.cmakeTarget,
           QStringLiteral("galleryComponentReferenceCMakeTarget"),
           true);

    applyPalette();
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

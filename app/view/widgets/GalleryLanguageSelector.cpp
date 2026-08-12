#include "GalleryLanguageSelector.h"

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QSizePolicy>

#include "components/basicinput/ToggleButton.h"
#include "design/Spacing.h"
#include "design/Typography.h"

namespace fluent::gallery {

GalleryLanguageSelector::GalleryLanguageSelector(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("galleryLanguageSelector"));
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setAccessibleName(QStringLiteral("Source language"));

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(::Spacing::XSmall);

    auto* group = new QButtonGroup(this);
    group->setExclusive(true);

    const auto makeButton = [this, layout, group](
                                const QString& text,
                                const QString& objectName,
                                int minimumWidth) {
        auto* button = new fluent::basicinput::ToggleButton(text, this);
        button->setObjectName(objectName);
        button->setFluentStyle(fluent::basicinput::Button::Subtle);
        button->setFluentSize(fluent::basicinput::Button::Small);
        button->setFluentLayout(fluent::basicinput::Button::TextOnly);
        button->setFont(themeFont(Typography::FontRole::Caption).toQFont());
        button->setMinimumWidth(minimumWidth);
        button->setFixedHeight(::Spacing::ControlHeight::Small);
        button->setFocusPolicy(Qt::StrongFocus);
        group->addButton(button);
        layout->addWidget(button);
        return button;
    };

    m_cppButton = makeButton(
        QStringLiteral("C++"),
        QStringLiteral("galleryLanguageCppButton"),
        48);
    m_pythonButton = makeButton(
        QStringLiteral("Python"),
        QStringLiteral("galleryLanguagePythonButton"),
        64);
    m_cppButton->setAccessibleName(QStringLiteral("Show C++ source"));
    m_pythonButton->setAccessibleName(QStringLiteral("Show Python source"));
    m_cppButton->setChecked(true);

    connect(m_cppButton,
            &fluent::basicinput::ToggleButton::toggled,
            this,
            [this](bool checked) {
                if (checked)
                    selectFromUser(GalleryCodeLanguage::Cpp);
            });
    connect(m_pythonButton,
            &fluent::basicinput::ToggleButton::toggled,
            this,
            [this](bool checked) {
                if (checked)
                    selectFromUser(GalleryCodeLanguage::Python);
            });
}

void GalleryLanguageSelector::setLanguage(GalleryCodeLanguage language)
{
    if (m_language == language)
        return;
    m_language = language;
    auto* target = language == GalleryCodeLanguage::Cpp
        ? m_cppButton
        : m_pythonButton;
    if (target)
        target->setChecked(true);
}

void GalleryLanguageSelector::onThemeUpdated()
{
    if (m_cppButton)
        m_cppButton->onThemeUpdated();
    if (m_pythonButton)
        m_pythonButton->onThemeUpdated();
}

void GalleryLanguageSelector::selectFromUser(
    GalleryCodeLanguage language)
{
    if (m_language == language)
        return;
    setLanguage(language);
    emit languageChanged(language);
}

} // namespace fluent::gallery

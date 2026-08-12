#ifndef GALLERYLANGUAGESELECTOR_H
#define GALLERYLANGUAGESELECTOR_H

#include <QWidget>

#include "components/foundation/FluentElement.h"
#include "view/support/GalleryCodeLanguage.h"

namespace fluent::basicinput {
class ToggleButton;
}

namespace fluent::gallery {

/**
 * @brief Compact mutually-exclusive C++ / Python selector for Gallery cards.
 * zh_CN: Gallery 卡片使用的紧凑 C++ / Python 互斥选择器。
 */
class GalleryLanguageSelector final : public QWidget,
                                      public fluent::FluentElement {
    Q_OBJECT

public:
    explicit GalleryLanguageSelector(QWidget* parent = nullptr);

    GalleryCodeLanguage language() const { return m_language; }
    void setLanguage(GalleryCodeLanguage language);

    fluent::basicinput::ToggleButton* cppButton() const
    {
        return m_cppButton;
    }
    fluent::basicinput::ToggleButton* pythonButton() const
    {
        return m_pythonButton;
    }

    void onThemeUpdated() override;

signals:
    void languageChanged(fluent::gallery::GalleryCodeLanguage language);

private:
    void selectFromUser(GalleryCodeLanguage language);

    GalleryCodeLanguage m_language = GalleryCodeLanguage::Cpp;
    fluent::basicinput::ToggleButton* m_cppButton = nullptr;
    fluent::basicinput::ToggleButton* m_pythonButton = nullptr;
};

} // namespace fluent::gallery

#endif // GALLERYLANGUAGESELECTOR_H

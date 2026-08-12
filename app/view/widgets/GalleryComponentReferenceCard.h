#ifndef GALLERYCOMPONENTREFERENCECARD_H
#define GALLERYCOMPONENTREFERENCECARD_H

#include <QFrame>
#include <QVector>

#include "components/foundation/FluentElement.h"
#include "model/GalleryComponentCatalog.h"
#include "view/support/GalleryCodeLanguage.h"

class QGridLayout;

namespace fluent::textfields {
class Label;
}

namespace fluent::gallery {

class GalleryLanguageSelector;

/**
 * @brief Compact public-API reference shown before a component's live examples.
 * zh_CN: 在组件实时示例前展示的紧凑公共 API 参考卡片。
 */
class GalleryComponentReferenceCard final : public QFrame, public fluent::FluentElement {
    Q_OBJECT

public:
    explicit GalleryComponentReferenceCard(const GalleryComponentReference& reference,
                                           QWidget* parent = nullptr);
    GalleryComponentReferenceCard(const GalleryComponentReference& reference,
                                  bool showLanguageSelector,
                                  QWidget* parent = nullptr);

    const GalleryComponentReference& reference() const { return m_reference; }
    GalleryCodeLanguage codeLanguage() const { return m_codeLanguage; }
    GalleryLanguageSelector* languageSelector() const
    {
        return m_languageSelector;
    }

    void setCodeLanguage(GalleryCodeLanguage language);

    void onThemeUpdated() override;

signals:
    void codeLanguageChanged(
        fluent::gallery::GalleryCodeLanguage language);

private:
    void addRow(QGridLayout* layout,
                int row,
                const QString& name,
                const QString& value,
                const QString& valueObjectName,
                bool codeValue);
    void updateReferenceRows();
    void applyPalette();

    GalleryComponentReference m_reference;
    GalleryCodeLanguage m_codeLanguage = GalleryCodeLanguage::Cpp;
    GalleryLanguageSelector* m_languageSelector = nullptr;
    QVector<fluent::textfields::Label*> m_nameLabels;
    QVector<fluent::textfields::Label*> m_valueLabels;
};

} // namespace fluent::gallery

#endif // GALLERYCOMPONENTREFERENCECARD_H

#ifndef GALLERYCOMPONENTREFERENCECARD_H
#define GALLERYCOMPONENTREFERENCECARD_H

#include <QFrame>
#include <QVector>

#include "components/foundation/FluentElement.h"
#include "model/GalleryComponentCatalog.h"

class QGridLayout;

namespace fluent::textfields {
class Label;
}

namespace fluent::gallery {

/**
 * @brief Compact public-API reference shown before a component's live examples.
 * zh_CN: 在组件实时示例前展示的紧凑公共 API 参考卡片。
 */
class GalleryComponentReferenceCard final : public QFrame, public fluent::FluentElement {
public:
    explicit GalleryComponentReferenceCard(const GalleryComponentReference& reference,
                                           QWidget* parent = nullptr);

    const GalleryComponentReference& reference() const { return m_reference; }

    void onThemeUpdated() override;

private:
    void addRow(QGridLayout* layout,
                int row,
                const QString& name,
                const QString& value,
                const QString& valueObjectName,
                bool codeValue);
    void applyPalette();

    GalleryComponentReference m_reference;
    QVector<fluent::textfields::Label*> m_nameLabels;
    QVector<fluent::textfields::Label*> m_valueLabels;
};

} // namespace fluent::gallery

#endif // GALLERYCOMPONENTREFERENCECARD_H

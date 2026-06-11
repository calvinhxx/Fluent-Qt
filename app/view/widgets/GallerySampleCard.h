#ifndef GALLERYSAMPLECARD_H
#define GALLERYSAMPLECARD_H

#include <QFrame>
#include <QSize>
#include <QString>

#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"
#include "model/GalleryContentCatalog.h"

namespace fluent::textfields {
class Label;
}

class QResizeEvent;

namespace fluent::gallery {

class GalleryCodeBlock;

/**
 * @brief Card shell that hosts one sample's title, description, live preview, and code.
 * zh_CN: 承载单个实样标题、描述、实时预览与代码的卡片外壳。
 */
class GallerySampleCard : public QFrame, public fluent::FluentElement, public fluent::QMLPlus {
public:
    explicit GallerySampleCard(const GallerySample& sample, QWidget* parent = nullptr);

    QString sampleId() const { return m_sampleId; }
    QWidget* previewWidget() const { return m_preview; }
    QWidget* optionsWidget() const { return m_options; }
    GalleryCodeBlock* codeBlock() const { return m_codeBlock; }
    fluent::textfields::Label* titleLabel() const { return m_titleLabel; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    void onThemeUpdated() override;

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void applyPalette();
    void updateAnchoredLayout();
    int preferredHeightForWidget(QWidget* widget, int width) const;
    int calculatedHeightForWidth(int width) const;
    int contentWidthForCardWidth(int width) const;

    QString m_sampleId;
    fluent::textfields::Label* m_titleLabel = nullptr;
    fluent::textfields::Label* m_descriptionLabel = nullptr;
    QFrame* m_previewSurface = nullptr;
    QWidget* m_preview = nullptr;
    QWidget* m_options = nullptr;
    GalleryCodeBlock* m_codeBlock = nullptr;
};

} // namespace fluent::gallery

#endif // GALLERYSAMPLECARD_H

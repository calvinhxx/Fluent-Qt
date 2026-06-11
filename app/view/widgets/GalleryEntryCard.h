#ifndef GALLERYENTRYCARD_H
#define GALLERYENTRYCARD_H

#include <QFrame>
#include <QString>

#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"

class QMouseEvent;

namespace fluent::textfields {
class Label;
}

namespace fluent::gallery {

class GalleryIconTile;

/**
 * @brief Clickable overview card that links to a target Gallery route.
 * zh_CN: 可点击的概览卡片，链接到目标 Gallery 路由。
 */
class GalleryEntryCard : public QFrame, public fluent::FluentElement, public fluent::QMLPlus {
    Q_OBJECT

public:
    GalleryEntryCard(const QString& targetRouteId,
                     const QString& title,
                     const QString& description,
                     QWidget* parent = nullptr);

    QString targetRouteId() const { return m_targetRouteId; }
    fluent::textfields::Label* titleLabel() const { return m_titleLabel; }

    /**
     * @brief Renders an icon-font glyph tile instead of the control image.
     * zh_CN: 以图标字体字形代替控件图片渲染图标块。
     */
    void setIconGlyph(const QString& glyph);

    void onThemeUpdated() override;

signals:
    /** @brief Emitted when the card is activated by the user. */
    void activated(const QString& routeId);

protected:
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    void applyPalette();

    QString m_targetRouteId;
    GalleryIconTile* m_iconTile = nullptr;
    fluent::textfields::Label* m_titleLabel = nullptr;
    fluent::textfields::Label* m_descriptionLabel = nullptr;
};

} // namespace fluent::gallery

#endif // GALLERYENTRYCARD_H

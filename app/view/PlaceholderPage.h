#ifndef PLACEHOLDERPAGE_H
#define PLACEHOLDERPAGE_H

#include <QColor>
#include <QWidget>

#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"
#include "model/GalleryNavigationItem.h"

namespace fluent::textfields {
class Label;
}

namespace fluent::gallery {

class PlaceholderPage : public QWidget, public FluentElement, public QMLPlus {
public:
    explicit PlaceholderPage(const GalleryNavigationItem& item, QWidget* parent = nullptr);

    QString routeId() const { return m_routeId; }
    QString title() const { return m_title; }
    QColor placeholderColor() const { return m_placeholderColor; }
    fluent::textfields::Label* titleLabel() const { return m_titleLabel; }

    void onThemeUpdated() override;

private:
    void applyPalette();
    QWidget* createPlaceholderCard(const QString& title, const QString& body);

    QString m_routeId;
    QString m_title;
    QColor m_placeholderColor;
    fluent::textfields::Label* m_titleLabel = nullptr;
};

} // namespace fluent::gallery

#endif // PLACEHOLDERPAGE_H

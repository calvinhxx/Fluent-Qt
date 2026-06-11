#ifndef SETTINGSPAGE_H
#define SETTINGSPAGE_H

#include <QWidget>

#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"
#include "model/GalleryNavigationItem.h"

namespace fluent::textfields {
class Label;
}

namespace fluent::gallery {

class SettingsPage : public QWidget, public FluentElement, public QMLPlus {
public:
    explicit SettingsPage(const GalleryNavigationItem& item, QWidget* parent = nullptr);

    QString routeId() const { return m_routeId; }
    fluent::textfields::Label* titleLabel() const { return m_titleLabel; }

    void onThemeUpdated() override;

private:
    QWidget* createSettingsRow(const QString& icon,
                               const QString& title,
                               const QString& subtitle,
                               QWidget* trailing);
    QWidget* createSectionTitle(const QString& title);
    QWidget* createChoiceButton(const QString& text);
    void applyPalette();

    QString m_routeId;
    fluent::textfields::Label* m_titleLabel = nullptr;
};

} // namespace fluent::gallery

#endif // SETTINGSPAGE_H

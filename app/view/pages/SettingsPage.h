#ifndef SETTINGSPAGE_H
#define SETTINGSPAGE_H

#include <QStringList>
#include <QUrl>
#include <QWidget>

#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"
#include "model/GalleryNavigationItem.h"
#include "viewmodel/UpdateChecker.h"

class QResizeEvent;
class QVBoxLayout;

namespace fluent::textfields {
class Label;
}

namespace fluent::basicinput {
class Button;
class ComboBox;
}

namespace fluent::gallery {

class AccentColorControl;

class SettingsPage : public QWidget, public FluentElement, public QMLPlus {
public:
    explicit SettingsPage(const GalleryNavigationItem& item, QWidget* parent = nullptr);

    QString routeId() const { return m_routeId; }
    fluent::textfields::Label* titleLabel() const { return m_titleLabel; }

    void onThemeUpdated() override;

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    QWidget* createSettingsRow(const QString& icon,
                               const QString& title,
                               const QString& subtitle,
                               QWidget* trailing);
    QWidget* createSectionTitle(const QString& title);
    fluent::basicinput::ComboBox* createChoiceBox(const QString& objectName,
                                                   const QStringList& choices,
                                                   int currentIndex);
    QWidget* createUpdateCheckControl();
    void showDisplayScaleRestartDialog();
    void startUpdateCheck();
    void handleUpdateCheckFinished(const UpdateChecker::Result& result);
    void openUpdateTarget();
    void applyPalette();
    void updateResponsiveLayout();

    QString m_routeId;
    QWidget* m_viewport = nullptr;
    QVBoxLayout* m_contentLayout = nullptr;
    fluent::textfields::Label* m_titleLabel = nullptr;
    fluent::basicinput::ComboBox* m_themeChoice = nullptr;
    fluent::basicinput::ComboBox* m_styleChoice = nullptr;
    AccentColorControl* m_accentControl = nullptr;
    fluent::basicinput::ComboBox* m_navigationChoice = nullptr;
    fluent::basicinput::ComboBox* m_effectChoice = nullptr;
    fluent::basicinput::ComboBox* m_displayScaleChoice = nullptr;
    fluent::basicinput::ComboBox* m_closeBehaviorChoice = nullptr;
    UpdateChecker* m_updateChecker = nullptr;
    fluent::textfields::Label* m_updateStatusLabel = nullptr;
    fluent::basicinput::Button* m_updateButton = nullptr;
    QUrl m_updateActionUrl;
};

} // namespace fluent::gallery

#endif // SETTINGSPAGE_H

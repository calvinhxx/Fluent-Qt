#ifndef FLUENTQT_COMPONENTS_MENUS_TOOLBARS_PRIVATE_COMMANDPRESENTER_P_H
#define FLUENTQT_COMPONENTS_MENUS_TOOLBARS_PRIVATE_COMMANDPRESENTER_P_H

#include <QIcon>
#include <QPointer>

#include <functional>

#include "components/basicinput/Button.h"

class QAction;
class QPaintEvent;

namespace fluent::menus_toolbars::detail {

// Private QAction presenter shared by the inline row and its overflow surface.
// The action is borrowed and remains the only source of command semantics.
class CommandPresenter final : public basicinput::Button {
public:
    enum class Mode {
        Primary,
        Overflow,
    };

    using ActivationHandler = std::function<void(QAction*)>;

    CommandPresenter(QAction* action,
                     Mode mode,
                     ActivationHandler activationHandler,
                     QWidget* parent = nullptr);

    QAction* action() const { return m_action.data(); }
    Mode mode() const { return m_mode; }

    void setPrimaryLabelCollapsed(bool collapsed);
    void synchronize();

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
    void onThemeUpdated() override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void paintOverflowContent();

    QPointer<QAction> m_action;
    Mode m_mode = Mode::Primary;
    ActivationHandler m_activationHandler;
    bool m_primaryLabelCollapsed = false;
    QString m_displayText;
    QString m_shortcutText;
    QIcon m_displayIcon;
};

// The More button is not backed by a synthetic QAction. Keeping it separate
// prevents it from leaking into the caller-owned semantic collections.
class CommandMoreButton final : public basicinput::Button {
public:
    explicit CommandMoreButton(QWidget* parent = nullptr);

    void setExpandedState(bool expanded, bool expandable);
};

} // namespace fluent::menus_toolbars::detail

#endif // FLUENTQT_COMPONENTS_MENUS_TOOLBARS_PRIVATE_COMMANDPRESENTER_P_H

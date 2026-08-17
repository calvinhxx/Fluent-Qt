#ifndef FLUENTQT_PYSIDE6_BINDINGAPI_H
#define FLUENTQT_PYSIDE6_BINDINGAPI_H

#include <QFont>
#include <QMargins>
#include <QObject>
#include <QPointer>
#include <QSizeF>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include <components/basicinput/Button.h>
#include <components/basicinput/CheckBox.h>
#include <components/basicinput/ColorPicker.h>
#include <components/basicinput/ComboBox.h>
#include <components/basicinput/CompoundButton.h>
#include <components/basicinput/DropDownButton.h>
#include <components/basicinput/HyperlinkButton.h>
#include <components/basicinput/RadioButton.h>
#include <components/basicinput/RatingControl.h>
#include <components/basicinput/RepeatButton.h>
#include <components/basicinput/Slider.h>
#include <components/basicinput/SplitButton.h>
#include <components/basicinput/ToggleButton.h>
#include <components/basicinput/ToggleSplitButton.h>
#include <components/basicinput/ToggleSwitch.h>
#include <components/collections/DataGrid.h>
#include <components/collections/DrawerView.h>
#include <components/collections/FlipView.h>
#include <components/collections/FlowView.h>
#include <components/collections/GridView.h>
#include <components/collections/ListView.h>
#include <components/collections/SplitView.h>
#include <components/collections/StackView.h>
#include <components/collections/TreeView.h>
#include <components/date_time/CalendarDatePicker.h>
#include <components/date_time/CalendarView.h>
#include <components/date_time/DatePicker.h>
#include <components/date_time/TimePicker.h>
#include <components/dialogs_flyouts/CoachMark.h>
#include <components/dialogs_flyouts/ContentDialog.h>
#include <components/dialogs_flyouts/Dialog.h>
#include <components/dialogs_flyouts/Flyout.h>
#include <components/dialogs_flyouts/Popup.h>
#include <components/dialogs_flyouts/TeachingTip.h>
#include <components/foundation/FluentElement.h>
#include <components/foundation/FontIcon.h>
#include <components/foundation/QMLPlus.h>
#include <components/foundation/UserTheme.h>
#include <components/layout/Accordion.h>
#include <components/layout/Card.h>
#include <components/layout/Divider.h>
#include <components/layout/Expander.h>
#include <components/layout/Field.h>
#include <components/menus_toolbars/CommandBar.h>
#include <components/menus_toolbars/CommandBarFlyout.h>
#include <components/menus_toolbars/Menu.h>
#include <components/menus_toolbars/MenuBar.h>
#include <components/navigation/Breadcrumb.h>
#include <components/navigation/NavigationView.h>
#include <components/navigation/Pivot.h>
#include <components/navigation/SelectorBar.h>
#include <components/navigation/StackContentHost.h>
#include <components/navigation/TabView.h>
#include <components/scrolling/AnnotatedScrollBar.h>
#include <components/scrolling/PipsPager.h>
#include <components/scrolling/ScrollBar.h>
#include <components/scrolling/ScrollView.h>
#include <components/status_info/Avatar.h>
#include <components/status_info/InfoBadge.h>
#include <components/status_info/InfoBar.h>
#include <components/status_info/ProgressBar.h>
#include <components/status_info/ProgressRing.h>
#include <components/status_info/Shimmer.h>
#include <components/status_info/Toast.h>
#include <components/status_info/ToolTip.h>
#include <components/textfields/AutoSuggestBox.h>
#include <components/textfields/EditingCommandRouter.h>
#include <components/textfields/Label.h>
#include <components/textfields/LineEdit.h>
#include <components/textfields/NumberBox.h>
#include <components/textfields/PasswordBox.h>
#include <components/textfields/TextEdit.h>
#include <components/windowing/TitleBar.h>
#include <components/windowing/Window.h>
#include <design/Typography.h>

namespace fluent::binding {

enum class Theme {
    Light,
    Dark
};

enum class SelectionMode {
    None,
    Single,
    Multiple,
    Extended
};

enum class BindingMode {
    OneWay,
    TwoWay
};

enum class AnchorEdge {
    None,
    Left,
    Right,
    Top,
    Bottom,
    HorizontalCenter,
    VerticalCenter
};

class AnchorLayout;
class AnchorSpecPrivate;

class AnchorSpec {
public:
    AnchorSpec();
    AnchorSpec(const AnchorSpec& other);
    AnchorSpec& operator=(const AnchorSpec& other);
    ~AnchorSpec();

    void setAnchor(AnchorEdge sourceEdge,
                   QWidget* target,
                   AnchorEdge targetEdge,
                   int offset = 0);
    void setFill(const QMargins& margins = QMargins());

private:
    friend class AnchorLayout;
    AnchorSpecPrivate* d_ptr;
};

class AnchorLayout : public QObject {
public:
    explicit AnchorLayout(QWidget* parent = nullptr);
    ~AnchorLayout() override;

    void addWidget(QWidget* widget, const AnchorSpec& anchors);

private:
    QPointer<fluent::AnchorLayout> m_layout;
};

class StateGroupPrivate;

class StateGroup : public QObject, public fluent::QMLPlus {
public:
    explicit StateGroup(QObject* parent = nullptr);
    ~StateGroup() override;

    bool addStateChange(const QString& name,
                        QObject* target,
                        const QString& propertyName,
                        const QVariant& value);
    void clearStateDefinition(const QString& name);
    bool hasState(const QString& name) const;
    void setState(const QString& name);
    QString state() const;

private:
    StateGroupPrivate* d_ptr;
};

class FluentWidget
    : public QWidget,
      public fluent::FluentElement,
      public fluent::QMLPlus {
public:
    explicit FluentWidget(QWidget* parent = nullptr);
    ~FluentWidget() override;

    QVariantMap themeTokensForBinding() const;
    QFont themeFontForBinding(
        Typography::FontRole role = Typography::FontRole::Body) const;
    fluent::binding::Theme effectiveThemeForBinding() const;
    void onThemeUpdated() override;
};

class ScrollViewZoomAwareWidget
    : public QWidget,
      public fluent::scrolling::ScrollViewZoomAware {
public:
    explicit ScrollViewZoomAwareWidget(QWidget* parent = nullptr);
    ~ScrollViewZoomAwareWidget() override;

    QSizeF scrollViewUnscaledSize() const override;
    void setScrollViewZoomFactor(qreal factor) override;

private:
    qreal m_zoomFactor = 1.0;
};

} // namespace fluent::binding

void prepareHighDpiApplication();
bool initializeResources();
QFont fontForRole(Typography::FontRole role);
void setTheme(fluent::binding::Theme theme);
fluent::binding::Theme currentTheme();
void applyUserTheme();
void setAccentColor(const QColor &color);
QColor accentColor();
void resetThemeTokens();
void setFontScale(qreal scale);
qreal fontScale();
QVariantMap themeTokensForWidgetForBinding(const QWidget* widget);
void refreshWidgetThemeForBinding(QWidget* widget);
bool bindProperties(QObject* source,
                    const QString& sourceProperty,
                    QObject* target,
                    const QString& targetProperty,
                    fluent::binding::BindingMode mode);
fluent::binding::SelectionMode flowViewSelectionMode(
    const fluent::collections::FlowView* view);
void setFlowViewSelectionMode(
    fluent::collections::FlowView* view,
    fluent::binding::SelectionMode mode);
fluent::scrolling::ScrollBar* flowViewVerticalFluentScrollBar(
    const fluent::collections::FlowView* view);
fluent::binding::SelectionMode dataGridSelectionMode(
    const fluent::collections::DataGrid* view);
void setDataGridSelectionMode(
    fluent::collections::DataGrid* view,
    fluent::binding::SelectionMode mode);
fluent::scrolling::ScrollBar* dataGridVerticalFluentScrollBar(
    const fluent::collections::DataGrid* view);
fluent::scrolling::ScrollBar* dataGridHorizontalFluentScrollBar(
    const fluent::collections::DataGrid* view);
fluent::binding::SelectionMode gridViewSelectionMode(
    const fluent::collections::GridView* view);
void setGridViewSelectionMode(
    fluent::collections::GridView* view,
    fluent::binding::SelectionMode mode);
fluent::scrolling::ScrollBar* gridViewVerticalFluentScrollBar(
    const fluent::collections::GridView* view);
fluent::binding::SelectionMode listViewSelectionMode(
    const fluent::collections::ListView* view);
void setListViewSelectionMode(
    fluent::collections::ListView* view,
    fluent::binding::SelectionMode mode);
fluent::scrolling::ScrollBar* listViewVerticalFluentScrollBar(
    const fluent::collections::ListView* view);
fluent::scrolling::ScrollBar* listViewHorizontalFluentScrollBar(
    const fluent::collections::ListView* view);
bool listViewSectionEnabled(const fluent::collections::ListView* view);
void setListViewSectionEnabled(
    fluent::collections::ListView* view,
    bool enabled);
void setListViewSectionKeys(
    fluent::collections::ListView* view,
    const QStringList& keys);
void clearListViewSectionKeyFunction(
    fluent::collections::ListView* view);
fluent::binding::SelectionMode treeViewSelectionMode(
    const fluent::collections::TreeView* view);
void setTreeViewSelectionMode(
    fluent::collections::TreeView* view,
    fluent::binding::SelectionMode mode);
fluent::scrolling::ScrollBar* treeViewVerticalFluentScrollBar(
    const fluent::collections::TreeView* view);
fluent::scrolling::ScrollBar* treeViewHorizontalFluentScrollBar(
    const fluent::collections::TreeView* view);
void setBreadcrumbTextItems(
    fluent::navigation::Breadcrumb* breadcrumb,
    const QStringList& items);
void setBreadcrumbMetadataItems(
    fluent::navigation::Breadcrumb* breadcrumb,
    const QVector<fluent::navigation::BreadcrumbItem>& items);
void setAnnotatedScrollBarDetailLabelText(
    fluent::scrolling::AnnotatedScrollBar* scrollBar,
    int offset,
    const QString& text);
void clearAnnotatedScrollBarDetailLabelProvider(
    fluent::scrolling::AnnotatedScrollBar* scrollBar);
bool annotatedScrollBarHasDetailLabelProvider(
    const fluent::scrolling::AnnotatedScrollBar* scrollBar);
QVariantList shimmerElementsForBinding(
    const fluent::status_info::Shimmer* shimmer);
bool setShimmerElementsJsonForBinding(
    fluent::status_info::Shimmer* shimmer,
    const QString& elementsJson);
void clearShimmerElementsForBinding(
    fluent::status_info::Shimmer* shimmer);
fluent::status_info::Toast* showToastForBinding(
    QWidget* host,
    QWidget* anchor,
    const QString& message,
    fluent::status_info::Toast::Severity severity,
    int durationMs,
    fluent::status_info::Toast::Placement placement,
    const QMargins& margins);
fluent::status_info::Toast* showOrUpdateToastForBinding(
    QWidget* host,
    QWidget* anchor,
    const QString& updateKey,
    const QString& message,
    fluent::status_info::Toast::Severity severity,
    int durationMs,
    fluent::status_info::Toast::Placement placement,
    const QMargins& margins);
int themeRevision();
QVariantMap bindingBuildInfo();

#endif // FLUENTQT_PYSIDE6_BINDINGAPI_H

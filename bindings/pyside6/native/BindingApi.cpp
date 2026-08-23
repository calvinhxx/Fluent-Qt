#include "BindingApi.h"

#include <FluentQt/Diagnostics.h>
#include <FluentQt/FluentQt.h>

#include <components/foundation/ThemeRegistry.h>
#include <design/Spacing.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaProperty>
#include <QSysInfo>
#include <QtGlobal>

static_assert(
    static_cast<int>(fluent::binding::Theme::Light) ==
    static_cast<int>(fluent::FluentElement::Light));
static_assert(
    static_cast<int>(fluent::binding::Theme::Dark) ==
    static_cast<int>(fluent::FluentElement::Dark));
static_assert(
    static_cast<int>(fluent::binding::SelectionMode::None) ==
    static_cast<int>(fluent::collections::SelectionMode::None));
static_assert(
    static_cast<int>(fluent::binding::SelectionMode::Single) ==
    static_cast<int>(fluent::collections::SelectionMode::Single));
static_assert(
    static_cast<int>(fluent::binding::SelectionMode::Multiple) ==
    static_cast<int>(fluent::collections::SelectionMode::Multiple));
static_assert(
    static_cast<int>(fluent::binding::SelectionMode::Extended) ==
    static_cast<int>(fluent::collections::SelectionMode::Extended));
static_assert(
    static_cast<int>(fluent::binding::BindingMode::OneWay) ==
    static_cast<int>(fluent::PropertyBinder::OneWay));
static_assert(
    static_cast<int>(fluent::binding::BindingMode::TwoWay) ==
    static_cast<int>(fluent::PropertyBinder::TwoWay));

namespace {

fluent::AnchorLayout::Edge toAnchorEdge(fluent::binding::AnchorEdge edge) {
  using BindingEdge = fluent::binding::AnchorEdge;
  using NativeEdge = fluent::AnchorLayout::Edge;
  switch (edge) {
    case BindingEdge::Left:
      return NativeEdge::Left;
    case BindingEdge::Right:
      return NativeEdge::Right;
    case BindingEdge::Top:
      return NativeEdge::Top;
    case BindingEdge::Bottom:
      return NativeEdge::Bottom;
    case BindingEdge::HorizontalCenter:
      return NativeEdge::HCenter;
    case BindingEdge::VerticalCenter:
      return NativeEdge::VCenter;
    case BindingEdge::None:
      break;
  }
  return NativeEdge::None;
}

QVariantMap colorTokens(const fluent::FluentElement::Colors& colors) {
  QVariantMap values;
#define FLUENTQT_COLOR_TOKEN(name) \
  values.insert(QStringLiteral(#name), colors.name)
  FLUENTQT_COLOR_TOKEN(accentDefault);
  FLUENTQT_COLOR_TOKEN(accentSecondary);
  FLUENTQT_COLOR_TOKEN(accentTertiary);
  FLUENTQT_COLOR_TOKEN(accentDisabled);
  FLUENTQT_COLOR_TOKEN(controlDefault);
  FLUENTQT_COLOR_TOKEN(controlSecondary);
  FLUENTQT_COLOR_TOKEN(controlTertiary);
  FLUENTQT_COLOR_TOKEN(controlDisabled);
  FLUENTQT_COLOR_TOKEN(controlAltSecondary);
  FLUENTQT_COLOR_TOKEN(controlAltTertiary);
  FLUENTQT_COLOR_TOKEN(subtleTransparent);
  FLUENTQT_COLOR_TOKEN(subtleSecondary);
  FLUENTQT_COLOR_TOKEN(subtleTertiary);
  FLUENTQT_COLOR_TOKEN(strokeDefault);
  FLUENTQT_COLOR_TOKEN(strokeSecondary);
  FLUENTQT_COLOR_TOKEN(strokeStrong);
  FLUENTQT_COLOR_TOKEN(strokeCard);
  FLUENTQT_COLOR_TOKEN(strokeDivider);
  FLUENTQT_COLOR_TOKEN(strokeSurface);
  FLUENTQT_COLOR_TOKEN(strokeFocusOuter);
  FLUENTQT_COLOR_TOKEN(strokeFocusInner);
  FLUENTQT_COLOR_TOKEN(textPrimary);
  FLUENTQT_COLOR_TOKEN(textSecondary);
  FLUENTQT_COLOR_TOKEN(textTertiary);
  FLUENTQT_COLOR_TOKEN(textDisabled);
  FLUENTQT_COLOR_TOKEN(textOnAccent);
  FLUENTQT_COLOR_TOKEN(textAccentPrimary);
  FLUENTQT_COLOR_TOKEN(bgCanvas);
  FLUENTQT_COLOR_TOKEN(bgLayer);
  FLUENTQT_COLOR_TOKEN(bgLayerAlt);
  FLUENTQT_COLOR_TOKEN(bgSolid);
  FLUENTQT_COLOR_TOKEN(grey10);
  FLUENTQT_COLOR_TOKEN(grey20);
  FLUENTQT_COLOR_TOKEN(grey30);
  FLUENTQT_COLOR_TOKEN(grey40);
  FLUENTQT_COLOR_TOKEN(grey50);
  FLUENTQT_COLOR_TOKEN(grey60);
  FLUENTQT_COLOR_TOKEN(grey90);
  FLUENTQT_COLOR_TOKEN(grey130);
  FLUENTQT_COLOR_TOKEN(grey160);
  FLUENTQT_COLOR_TOKEN(grey190);
  FLUENTQT_COLOR_TOKEN(systemCritical);
  FLUENTQT_COLOR_TOKEN(systemCriticalBg);
  FLUENTQT_COLOR_TOKEN(systemCaution);
  FLUENTQT_COLOR_TOKEN(systemCautionBg);
  FLUENTQT_COLOR_TOKEN(systemInfo);
  FLUENTQT_COLOR_TOKEN(systemInfoBg);
  FLUENTQT_COLOR_TOKEN(systemSuccess);
  FLUENTQT_COLOR_TOKEN(systemSuccessBg);
  FLUENTQT_COLOR_TOKEN(bgLayerOverlay);
#undef FLUENTQT_COLOR_TOKEN
  QVariantList charts;
  charts.reserve(colors.charts.size());
  for (const QColor& color : colors.charts)
    charts.append(color);
  values.insert(QStringLiteral("charts"), charts);
  return values;
}

QVariantMap spacingTokens(const fluent::FluentElement::Spacing& spacing) {
  QVariantMap padding;
  padding.insert(QStringLiteral("controlH"), spacing.padding.controlH);
  padding.insert(QStringLiteral("controlV"), spacing.padding.controlV);
  padding.insert(QStringLiteral("card"), spacing.padding.card);
  padding.insert(QStringLiteral("dialog"), spacing.padding.dialog);
  padding.insert(QStringLiteral("textFieldH"), spacing.padding.textFieldH);
  padding.insert(QStringLiteral("textFieldV"), spacing.padding.textFieldV);
  padding.insert(QStringLiteral("listItemH"), spacing.padding.listItemH);
  padding.insert(QStringLiteral("listItemV"), spacing.padding.listItemV);

  QVariantMap gap;
  gap.insert(QStringLiteral("tight"), spacing.gap.tight);
  gap.insert(QStringLiteral("normal"), spacing.gap.normal);
  gap.insert(QStringLiteral("loose"), spacing.gap.loose);
  gap.insert(QStringLiteral("section"), spacing.gap.section);

  QVariantMap controlHeight;
  controlHeight.insert(QStringLiteral("small"), spacing.controlHeight.small);
  controlHeight.insert(QStringLiteral("standard"),
                       spacing.controlHeight.standard);
  controlHeight.insert(QStringLiteral("large"), spacing.controlHeight.large);

  QVariantMap border;
  border.insert(QStringLiteral("normal"), ::Spacing::Border::Normal);
  border.insert(QStringLiteral("focused"), ::Spacing::Border::Focused);

  QVariantMap values;
  values.insert(QStringLiteral("padding"), padding);
  values.insert(QStringLiteral("gap"), gap);
  values.insert(QStringLiteral("controlHeight"), controlHeight);
  values.insert(QStringLiteral("border"), border);
  values.insert(QStringLiteral("xSmall"), spacing.xSmall);
  values.insert(QStringLiteral("small"), spacing.small);
  values.insert(QStringLiteral("medium"), spacing.medium);
  values.insert(QStringLiteral("standard"), spacing.standard);
  values.insert(QStringLiteral("large"), spacing.large);
  values.insert(QStringLiteral("xLarge"), spacing.xLarge);
  values.insert(QStringLiteral("xxLarge"), spacing.xxLarge);
  return values;
}

const fluent::FluentElement* nearestFluentElement(const QWidget* widget) {
  for (const QWidget* node = widget; node; node = node->parentWidget()) {
    if (const auto* element =
            dynamic_cast<const fluent::FluentElement*>(node)) {
      return element;
    }
  }
  return nullptr;
}

QVariantMap themeTokens(const fluent::FluentElement& element) {
  const auto radius = element.themeRadius();
  QVariantMap radiusValues;
  radiusValues.insert(QStringLiteral("none"), radius.none);
  radiusValues.insert(QStringLiteral("control"), radius.control);
  radiusValues.insert(QStringLiteral("overlay"), radius.overlay);

  const auto animation = element.themeAnimation();
  QVariantMap durations;
  durations.insert(QStringLiteral("fast"), animation.fast);
  durations.insert(QStringLiteral("normal"), animation.normal);
  durations.insert(QStringLiteral("slow"), animation.slow);
  durations.insert(QStringLiteral("verySlow"), animation.verySlow);
  QVariantMap easings;
  easings.insert(QStringLiteral("standard"),
                 QVariant::fromValue(animation.standard));
  easings.insert(QStringLiteral("accelerate"),
                 QVariant::fromValue(animation.accelerate));
  easings.insert(QStringLiteral("decelerate"),
                 QVariant::fromValue(animation.decelerate));
  easings.insert(QStringLiteral("entrance"),
                 QVariant::fromValue(animation.entrance));
  easings.insert(QStringLiteral("exit"),
                 QVariant::fromValue(animation.exit));
  QVariantMap animationValues;
  animationValues.insert(QStringLiteral("duration"), durations);
  animationValues.insert(QStringLiteral("easing"), easings);

  const auto acrylic = element.themeAcrylic();
  QVariantMap acrylicValues;
  acrylicValues.insert(QStringLiteral("tintColor"), acrylic.tintColor);
  acrylicValues.insert(QStringLiteral("tintOpacity"), acrylic.tintOpacity);
  acrylicValues.insert(QStringLiteral("luminosityOpacity"),
                       acrylic.luminosityOpacity);
  acrylicValues.insert(QStringLiteral("blurRadius"), acrylic.blurRadius);
  const auto mica = element.themeMica();
  QVariantMap micaValues;
  micaValues.insert(QStringLiteral("baseColor"), mica.baseColor);
  micaValues.insert(QStringLiteral("opacity"), mica.opacity);
  const auto smoke = element.themeSmoke();
  QVariantMap smokeValues;
  smokeValues.insert(QStringLiteral("baseColor"), smoke.baseColor);
  smokeValues.insert(QStringLiteral("opacity"), smoke.opacity);
  QVariantMap materialValues;
  materialValues.insert(QStringLiteral("acrylic"), acrylicValues);
  materialValues.insert(QStringLiteral("mica"), micaValues);
  materialValues.insert(QStringLiteral("smoke"), smokeValues);

  auto shadowValues = [&element](Elevation::Level level) {
    const auto shadow = element.themeShadow(level);
    QVariantMap values;
    values.insert(QStringLiteral("offsetX"), shadow.offsetX);
    values.insert(QStringLiteral("offsetY"), shadow.offsetY);
    values.insert(QStringLiteral("blurRadius"), shadow.blurRadius);
    values.insert(QStringLiteral("spreadRadius"), shadow.spreadRadius);
    values.insert(QStringLiteral("color"), shadow.color);
    values.insert(QStringLiteral("opacity"), shadow.opacity);
    return values;
  };
  QVariantMap elevationValues;
  elevationValues.insert(QStringLiteral("none"),
                         shadowValues(Elevation::None));
  elevationValues.insert(QStringLiteral("low"),
                         shadowValues(Elevation::Low));
  elevationValues.insert(QStringLiteral("medium"),
                         shadowValues(Elevation::Medium));
  elevationValues.insert(QStringLiteral("high"),
                         shadowValues(Elevation::High));
  elevationValues.insert(QStringLiteral("veryHigh"),
                         shadowValues(Elevation::VeryHigh));

  QVariantMap breakpointValues;
  breakpointValues.insert(
      QStringLiteral("small"),
      element.themeBreakpoint(Breakpoints::Breakpoint::Small));
  breakpointValues.insert(
      QStringLiteral("medium"),
      element.themeBreakpoint(Breakpoints::Breakpoint::Medium));
  breakpointValues.insert(
      QStringLiteral("large"),
      element.themeBreakpoint(Breakpoints::Breakpoint::Large));

  QVariantMap backdropValues;
  backdropValues.insert(QStringLiteral("active"),
                        element.themeBackdrop(true));
  backdropValues.insert(QStringLiteral("inactive"),
                        element.themeBackdrop(false));

  QVariantMap values;
  values.insert(QStringLiteral("theme"),
                static_cast<int>(element.effectiveTheme()));
  values.insert(QStringLiteral("colors"), colorTokens(element.themeColors()));
  values.insert(QStringLiteral("radius"), radiusValues);
  values.insert(QStringLiteral("spacing"),
                spacingTokens(element.themeSpacing()));
  values.insert(QStringLiteral("animation"), animationValues);
  values.insert(QStringLiteral("material"), materialValues);
  values.insert(QStringLiteral("elevation"), elevationValues);
  values.insert(QStringLiteral("breakpoints"), breakpointValues);
  values.insert(QStringLiteral("backdrop"), backdropValues);
  return values;
}

void refreshFluentSubtree(QWidget* root) {
  if (!root)
    return;

  if (auto* element = dynamic_cast<fluent::FluentElement*>(root))
    element->onThemeUpdated();
  root->update();

  const auto children = root->children();
  for (QObject* child : children) {
    if (auto* childWidget = qobject_cast<QWidget*>(child))
      refreshFluentSubtree(childWidget);
  }
}

} // namespace

class fluent::binding::AnchorSpecPrivate {
public:
  fluent::AnchorLayout::Anchors anchors;
};

fluent::binding::AnchorSpec::AnchorSpec()
    : d_ptr(new AnchorSpecPrivate) {}

fluent::binding::AnchorSpec::AnchorSpec(const AnchorSpec& other)
    : d_ptr(new AnchorSpecPrivate(*other.d_ptr)) {}

fluent::binding::AnchorSpec& fluent::binding::AnchorSpec::operator=(
    const AnchorSpec& other) {
  if (this != &other)
    *d_ptr = *other.d_ptr;
  return *this;
}

fluent::binding::AnchorSpec::~AnchorSpec() { delete d_ptr; }

void fluent::binding::AnchorSpec::setAnchor(
    AnchorEdge sourceEdge,
    QWidget* target,
    AnchorEdge targetEdge,
    int offset) {
  const fluent::AnchorLayout::Anchor anchor(
      target, toAnchorEdge(targetEdge), offset);
  switch (sourceEdge) {
    case AnchorEdge::Left:
      d_ptr->anchors.left = anchor;
      break;
    case AnchorEdge::Right:
      d_ptr->anchors.right = anchor;
      break;
    case AnchorEdge::Top:
      d_ptr->anchors.top = anchor;
      break;
    case AnchorEdge::Bottom:
      d_ptr->anchors.bottom = anchor;
      break;
    case AnchorEdge::HorizontalCenter:
      d_ptr->anchors.horizontalCenter = anchor;
      break;
    case AnchorEdge::VerticalCenter:
      d_ptr->anchors.verticalCenter = anchor;
      break;
    case AnchorEdge::None:
      break;
  }
}

void fluent::binding::AnchorSpec::setFill(const QMargins& margins) {
  d_ptr->anchors.fill = true;
  d_ptr->anchors.fillMargins = margins;
}

fluent::binding::AnchorLayout::AnchorLayout(QWidget* parent)
    : QObject(parent), m_layout(new fluent::AnchorLayout(parent)) {}

fluent::binding::AnchorLayout::~AnchorLayout() {
  delete m_layout;
}

void fluent::binding::AnchorLayout::addWidget(
    QWidget* widget,
    const AnchorSpec& anchors) {
  if (m_layout)
    m_layout->addAnchoredWidget(widget, anchors.d_ptr->anchors);
}

class fluent::binding::StateGroupPrivate {
public:
  QMap<QString, fluent::QMLState> states;
};

fluent::binding::StateGroup::StateGroup(QObject* parent)
    : QObject(parent), d_ptr(new StateGroupPrivate) {}

fluent::binding::StateGroup::~StateGroup() { delete d_ptr; }

bool fluent::binding::StateGroup::addStateChange(
    const QString& name,
    QObject* target,
    const QString& propertyName,
    const QVariant& value) {
  if (name.isEmpty() || !target || propertyName.isEmpty())
    return false;
  const QByteArray property = propertyName.toUtf8();
  const int propertyIndex = target->metaObject()->indexOfProperty(property);
  if (propertyIndex < 0
      && !target->dynamicPropertyNames().contains(property)) {
    return false;
  }
  if (propertyIndex >= 0
      && !target->metaObject()->property(propertyIndex).isWritable()) {
    return false;
  }

  fluent::QMLState& state = d_ptr->states[name];
  state.name = name;
  for (fluent::PropertyChange& change : state.changes) {
    if (change.target == target && change.propertyName == property) {
      change.value = value;
      fluent::QMLPlus::addState(state);
      return true;
    }
  }
  state.changes.append({target, property, value});
  fluent::QMLPlus::addState(state);
  return true;
}

void fluent::binding::StateGroup::clearStateDefinition(
    const QString& name) {
  if (name.isEmpty())
    return;
  fluent::QMLState& state = d_ptr->states[name];
  state.name = name;
  state.changes.clear();
  fluent::QMLPlus::addState(state);
}

bool fluent::binding::StateGroup::hasState(const QString& name) const {
  return d_ptr->states.contains(name);
}

void fluent::binding::StateGroup::setState(const QString& name) {
  fluent::QMLPlus::setState(name);
}

QString fluent::binding::StateGroup::state() const {
  return fluent::QMLPlus::state();
}

fluent::binding::FluentWidget::FluentWidget(QWidget* parent)
    : QWidget(parent) {}

fluent::binding::FluentWidget::~FluentWidget() = default;

QVariantMap fluent::binding::FluentWidget::themeTokensForBinding() const {
  return themeTokens(*this);
}

QFont fluent::binding::FluentWidget::themeFontForBinding(
    Typography::FontRole role) const {
  return themeFont(role).toQFont();
}

fluent::binding::Theme
fluent::binding::FluentWidget::effectiveThemeForBinding() const {
  return static_cast<fluent::binding::Theme>(
      static_cast<int>(effectiveTheme()));
}

void fluent::binding::FluentWidget::onThemeUpdated() { update(); }

fluent::binding::ScrollViewZoomAwareWidget::ScrollViewZoomAwareWidget(
    QWidget* parent)
    : QWidget(parent) {}

fluent::binding::ScrollViewZoomAwareWidget::~ScrollViewZoomAwareWidget() =
    default;

QSizeF fluent::binding::ScrollViewZoomAwareWidget::scrollViewUnscaledSize()
    const {
  return QSizeF(size()) / m_zoomFactor;
}

void fluent::binding::ScrollViewZoomAwareWidget::setScrollViewZoomFactor(
    qreal factor) {
  if (factor <= 0.0)
    return;
  const QSizeF unscaled = scrollViewUnscaledSize();
  m_zoomFactor = factor;
  resize((unscaled * factor).toSize());
}

void prepareHighDpiApplication() { fluent::prepareHighDpiApplication(); }

bool initializeResources() { return fluent::initializeResources(); }

QFont fontForRole(Typography::FontRole role) {
  return fluent::ThemeRegistry::instance().resolvedFontStyle(role).toQFont();
}

void setTheme(fluent::binding::Theme theme) {
  fluent::FluentElement::setTheme(
      static_cast<fluent::FluentElement::Theme>(static_cast<int>(theme)));
}

fluent::binding::Theme currentTheme() {
  return static_cast<fluent::binding::Theme>(
      static_cast<int>(fluent::FluentElement::currentTheme()));
}

void applyUserTheme() {
  fluent::UserTheme::apply();
}

void setAccentColor(const QColor &color) {
  fluent::UserTheme::applyAccentOverride(color);
}

QColor accentColor() {
  const bool dark =
      fluent::FluentElement::currentTheme() == fluent::FluentElement::Dark;
  return fluent::ThemeRegistry::instance().colors(dark).accentDefault;
}

void resetThemeTokens() { fluent::ThemeRegistry::instance().resetToDefaults(); }

void setFontScale(qreal scale) {
  fluent::ThemeRegistry::instance().setFontScale(scale);
}

qreal fontScale() { return fluent::ThemeRegistry::instance().fontScale(); }

QVariantMap themeTokensForWidgetForBinding(const QWidget* widget) {
  const auto* element = nearestFluentElement(widget);
  return element ? themeTokens(*element) : QVariantMap{};
}

QVariantMap inspectWidgetForBinding(QWidget* widget,
                                    int minimumHitWidth,
                                    int minimumHitHeight,
                                    int spacingGrid,
                                    bool checkClippedText,
                                    bool checkAccessibilityNames,
                                    bool checkHitAreas,
                                    bool checkFocusOrder,
                                    bool checkDuplicateActions,
                                    bool checkNestedScrolling,
                                    bool checkLayoutGrid) {
  fluent::diagnostics::InspectorOptions options;
  options.minimumHitArea =
      QSize(qMax(1, minimumHitWidth), qMax(1, minimumHitHeight));
  options.spacingGrid = qMax(1, spacingGrid);
  options.checkClippedText = checkClippedText;
  options.checkAccessibilityNames = checkAccessibilityNames;
  options.checkHitAreas = checkHitAreas;
  options.checkFocusOrder = checkFocusOrder;
  options.checkDuplicateActions = checkDuplicateActions;
  options.checkNestedScrolling = checkNestedScrolling;
  options.checkLayoutGrid = checkLayoutGrid;
  return fluent::diagnostics::Inspector::report(widget, options).toVariantMap();
}

void refreshWidgetThemeForBinding(QWidget* widget) {
  refreshFluentSubtree(widget);
}

bool bindProperties(QObject* source,
                    const QString& sourceProperty,
                    QObject* target,
                    const QString& targetProperty,
                    fluent::binding::BindingMode mode) {
  if (!source || !target || sourceProperty.isEmpty()
      || targetProperty.isEmpty()) {
    return false;
  }
  const QByteArray sourceName = sourceProperty.toUtf8();
  const QByteArray targetName = targetProperty.toUtf8();
  const int sourceIndex = source->metaObject()->indexOfProperty(sourceName);
  const int targetIndex = target->metaObject()->indexOfProperty(targetName);
  if (sourceIndex < 0 || targetIndex < 0)
    return false;

  const QMetaProperty sourceMeta =
      source->metaObject()->property(sourceIndex);
  const QMetaProperty targetMeta =
      target->metaObject()->property(targetIndex);
  if (!sourceMeta.hasNotifySignal() || !targetMeta.isWritable())
    return false;
  if (mode == fluent::binding::BindingMode::TwoWay
      && (!targetMeta.hasNotifySignal() || !sourceMeta.isWritable())) {
    return false;
  }

  fluent::PropertyBinder::bind(
      source,
      sourceName.constData(),
      target,
      targetName.constData(),
      static_cast<fluent::PropertyBinder::Direction>(
          static_cast<int>(mode)));
  return true;
}

fluent::binding::SelectionMode flowViewSelectionMode(
    const fluent::collections::FlowView* view) {
  return static_cast<fluent::binding::SelectionMode>(
      static_cast<int>(view->selectionMode()));
}

void setFlowViewSelectionMode(
    fluent::collections::FlowView* view,
    fluent::binding::SelectionMode mode) {
  view->setSelectionMode(
      static_cast<fluent::collections::SelectionMode>(
          static_cast<int>(mode)));
}

fluent::scrolling::ScrollBar* flowViewVerticalFluentScrollBar(
    const fluent::collections::FlowView* view) {
  return view->verticalFluentScrollBar();
}

fluent::binding::SelectionMode dataGridSelectionMode(
    const fluent::collections::DataGrid* view) {
  return static_cast<fluent::binding::SelectionMode>(
      static_cast<int>(view->selectionMode()));
}

void setDataGridSelectionMode(
    fluent::collections::DataGrid* view,
    fluent::binding::SelectionMode mode) {
  view->setSelectionMode(
      static_cast<fluent::collections::SelectionMode>(
          static_cast<int>(mode)));
}

fluent::scrolling::ScrollBar* dataGridVerticalFluentScrollBar(
    const fluent::collections::DataGrid* view) {
  return view->verticalFluentScrollBar();
}

fluent::scrolling::ScrollBar* dataGridHorizontalFluentScrollBar(
    const fluent::collections::DataGrid* view) {
  return view->horizontalFluentScrollBar();
}

fluent::binding::SelectionMode gridViewSelectionMode(
    const fluent::collections::GridView* view) {
  return static_cast<fluent::binding::SelectionMode>(
      static_cast<int>(view->selectionMode()));
}

void setGridViewSelectionMode(
    fluent::collections::GridView* view,
    fluent::binding::SelectionMode mode) {
  view->setSelectionMode(
      static_cast<fluent::collections::SelectionMode>(
          static_cast<int>(mode)));
}

fluent::scrolling::ScrollBar* gridViewVerticalFluentScrollBar(
    const fluent::collections::GridView* view) {
  return view->verticalFluentScrollBar();
}

fluent::binding::SelectionMode listViewSelectionMode(
    const fluent::collections::ListView* view) {
  return static_cast<fluent::binding::SelectionMode>(
      static_cast<int>(view->selectionMode()));
}

void setListViewSelectionMode(
    fluent::collections::ListView* view,
    fluent::binding::SelectionMode mode) {
  view->setSelectionMode(
      static_cast<fluent::collections::SelectionMode>(
          static_cast<int>(mode)));
}

fluent::scrolling::ScrollBar* listViewVerticalFluentScrollBar(
    const fluent::collections::ListView* view) {
  return view->verticalFluentScrollBar();
}

fluent::scrolling::ScrollBar* listViewHorizontalFluentScrollBar(
    const fluent::collections::ListView* view) {
  return view->horizontalFluentScrollBar();
}

bool listViewSectionEnabled(const fluent::collections::ListView* view) {
  return view && view->sectionEnabled();
}

void setListViewSectionEnabled(
    fluent::collections::ListView* view,
    bool enabled) {
  if (view)
    view->setSectionEnabled(enabled);
}

void setListViewSectionKeys(
    fluent::collections::ListView* view,
    const QStringList& keys) {
  if (!view)
    return;
  view->setSectionKeyFunction([keys](int row) {
    return row >= 0 && row < keys.size() ? keys.at(row) : QString();
  });
}

void clearListViewSectionKeyFunction(
    fluent::collections::ListView* view) {
  if (view)
    view->setSectionKeyFunction({});
}

fluent::binding::SelectionMode treeViewSelectionMode(
    const fluent::collections::TreeView* view) {
  return static_cast<fluent::binding::SelectionMode>(
      static_cast<int>(view->selectionMode()));
}

void setTreeViewSelectionMode(
    fluent::collections::TreeView* view,
    fluent::binding::SelectionMode mode) {
  view->setSelectionMode(
      static_cast<fluent::collections::SelectionMode>(
          static_cast<int>(mode)));
}

fluent::scrolling::ScrollBar* treeViewVerticalFluentScrollBar(
    const fluent::collections::TreeView* view) {
  return view->verticalFluentScrollBar();
}

fluent::scrolling::ScrollBar* treeViewHorizontalFluentScrollBar(
    const fluent::collections::TreeView* view) {
  return view->horizontalFluentScrollBar();
}

void setBreadcrumbTextItems(
    fluent::navigation::Breadcrumb* breadcrumb,
    const QStringList& items) {
  if (breadcrumb)
    breadcrumb->setItems(items);
}

void setBreadcrumbMetadataItems(
    fluent::navigation::Breadcrumb* breadcrumb,
    const QVector<fluent::navigation::BreadcrumbItem>& items) {
  if (breadcrumb)
    breadcrumb->setItems(items);
}

void setAnnotatedScrollBarDetailLabelText(
    fluent::scrolling::AnnotatedScrollBar* scrollBar,
    int offset,
    const QString& text) {
  if (!scrollBar)
    return;
  scrollBar->setDetailLabelProvider([offset, text](int requestedOffset) {
    return requestedOffset == offset ? text : QString();
  });
}

void clearAnnotatedScrollBarDetailLabelProvider(
    fluent::scrolling::AnnotatedScrollBar* scrollBar) {
  if (scrollBar)
    scrollBar->clearDetailLabelProvider();
}

bool annotatedScrollBarHasDetailLabelProvider(
    const fluent::scrolling::AnnotatedScrollBar* scrollBar) {
  return scrollBar && scrollBar->hasDetailLabelProvider();
}

QVariantList shimmerElementsForBinding(
    const fluent::status_info::Shimmer* shimmer) {
  QVariantList values;
  if (!shimmer)
    return values;
  const auto elements = shimmer->elements();
  values.reserve(elements.size());
  for (const auto& element : elements) {
    QVariantMap value;
    value.insert(QStringLiteral("shape"), static_cast<int>(element.shape));
    value.insert(QStringLiteral("rect"), element.rect);
    value.insert(QStringLiteral("radius"), element.radius);
    values.append(value);
  }
  return values;
}

bool setShimmerElementsJsonForBinding(
    fluent::status_info::Shimmer* shimmer,
    const QString& elementsJson) {
  if (!shimmer)
    return false;
  QJsonParseError parseError;
  const QJsonDocument document = QJsonDocument::fromJson(
      elementsJson.toUtf8(), &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isArray())
    return false;
  QVector<fluent::status_info::ShimmerPainter::Element> elements;
  const QJsonArray values = document.array();
  elements.reserve(values.size());
  for (const QJsonValue& value : values) {
    if (!value.isObject())
      return false;
    const QJsonObject fields = value.toObject();
    const int shapeValue = fields.value(QStringLiteral("shape")).toInt(1);
    const auto shape = static_cast<fluent::status_info::ShimmerPainter::Shape>(
        qBound(0, shapeValue, 3));
    const QRectF rect(fields.value(QStringLiteral("x")).toDouble(),
                      fields.value(QStringLiteral("y")).toDouble(),
                      fields.value(QStringLiteral("width")).toDouble(),
                      fields.value(QStringLiteral("height")).toDouble());
    const qreal radius = fields.value(QStringLiteral("radius")).toDouble(-1.0);
    elements.append({shape, rect, radius});
  }
  shimmer->setElements(elements);
  return true;
}

void clearShimmerElementsForBinding(
    fluent::status_info::Shimmer* shimmer) {
  if (shimmer)
    shimmer->clearElements();
}

fluent::status_info::Toast* showToastForBinding(
    QWidget* host,
    QWidget* anchor,
    const QString& message,
    fluent::status_info::Toast::Severity severity,
    int durationMs,
    fluent::status_info::Toast::Placement placement,
    const QMargins& margins) {
  if (!host || !anchor || anchor->window() != host)
    return nullptr;
  return fluent::status_info::Toast::showToast(
      anchor, message, severity, durationMs, placement, margins);
}

fluent::status_info::Toast* showOrUpdateToastForBinding(
    QWidget* host,
    QWidget* anchor,
    const QString& updateKey,
    const QString& message,
    fluent::status_info::Toast::Severity severity,
    int durationMs,
    fluent::status_info::Toast::Placement placement,
    const QMargins& margins) {
  if (!host || !anchor || anchor->window() != host)
    return nullptr;
  return fluent::status_info::Toast::showOrUpdateToast(
      anchor,
      updateKey,
      message,
      severity,
      durationMs,
      placement,
      margins);
}

int themeRevision() { return fluent::ThemeRegistry::instance().revision(); }

QVariantMap bindingBuildInfo() {
  QVariantMap info;
  info.insert(QStringLiteral("fluentqt_version"),
              QStringLiteral(FLUENT_QT_PYSIDE6_PROJECT_VERSION));
  info.insert(QStringLiteral("pyside6_version"),
              QStringLiteral(FLUENT_QT_PYSIDE6_VERSION));
  info.insert(QStringLiteral("shiboken6_version"),
              QStringLiteral(FLUENT_QT_SHIBOKEN6_VERSION));
  info.insert(QStringLiteral("shiboken6_generator_version"),
              QStringLiteral(FLUENT_QT_SHIBOKEN6_GENERATOR_VERSION));
  info.insert(QStringLiteral("qt_compile_version"),
              QStringLiteral(QT_VERSION_STR));
  info.insert(QStringLiteral("qt_runtime_version"),
              QString::fromLatin1(qVersion()));
  info.insert(QStringLiteral("architecture"),
              QSysInfo::currentCpuArchitecture());
  return info;
}

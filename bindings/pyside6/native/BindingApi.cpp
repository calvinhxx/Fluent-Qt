#include "BindingApi.h"

#include <FluentQt/FluentQt.h>

#include <components/foundation/ThemeRegistry.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSysInfo>
#include <QtGlobal>

static_assert(
    static_cast<int>(fluent::binding::Theme::Light) ==
    static_cast<int>(fluent::FluentElement::Light));
static_assert(
    static_cast<int>(fluent::binding::Theme::Dark) ==
    static_cast<int>(fluent::FluentElement::Dark));
static_assert(
    static_cast<int>(fluent::binding::DesignLanguage::DesignFluent) ==
    static_cast<int>(fluent::FluentElement::DesignFluent));
static_assert(
    static_cast<int>(fluent::binding::DesignLanguage::DesignMaterial) ==
    static_cast<int>(fluent::FluentElement::DesignMaterial));
static_assert(
    static_cast<int>(fluent::binding::DesignLanguage::DesignCupertino) ==
    static_cast<int>(fluent::FluentElement::DesignCupertino));
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

void applyStyleTheme(fluent::StyleTheme theme) {
  fluent::StyleThemeCatalog::apply(theme);
}

void setAccentColor(const QColor &color) {
  fluent::StyleThemeCatalog::applyAccentOverride(color);
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

fluent::binding::DesignLanguage currentDesignLanguage() {
  return static_cast<fluent::binding::DesignLanguage>(
      static_cast<int>(fluent::ThemeRegistry::instance().designLanguage()));
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

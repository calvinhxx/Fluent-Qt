#include "GalleryNavigationPane.h"

#include <cmath>
#include <QAbstractItemView>
#include <QEvent>
#include <QHelpEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPropertyAnimation>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QTimer>
#include <QVariant>
#include <QVBoxLayout>

#include "components/dialogs_flyouts/Popup.h"
#include "components/foundation/overlay/OverlayGeometry.h"
#include "components/collections/TreeView.h"
#include "components/scrolling/ScrollBar.h"
#include "components/status_info/ToolTip.h"
#include "utils/Log.h"
#include "GalleryCompactFlyout.h"
#include "GalleryNavigationDelegate.h"
#include "GalleryNavigationMetrics.h"
#include "view/support/GalleryStyleSupport.h"

namespace fluent::gallery {

namespace {

// A 1px footer separator that paints itself with CompositionMode_Source, so its semi-transparent
// stroke REPLACES the backing store each frame instead of compositing over it. Under a translucent
// top-level (macOS vibrancy) the backing isn't auto-cleared, so a plain translucent-background
// QWidget would stack alpha on every repaint and darken toward black as the user interacts.
// zh_CN: 1px 页脚分隔线，用 CompositionMode_Source 自绘，使其半透明描边每帧「替换」后备缓冲而非在其上叠加。
// 半透明顶层（macOS vibrancy）下后备缓冲不会自动清除，普通半透明背景的 QWidget 会逐帧叠加 alpha，随交互发黑。
class FooterDividerLine : public QWidget {
public:
    explicit FooterDividerLine(QWidget* parent)
        : QWidget(parent) {
        // Intentionally no WA_TranslucentBackground: on macOS it promotes the line to its own
        // layer that, before the window's vibrancy composites at startup, shows the tint at full
        // strength (a bright/white line). As a plain child it paints into the shared backing and
        // composites over the same vibrancy as the rest of the pane.
        // zh_CN: 故意不设 WA_TranslucentBackground：macOS 上它会把线提升为独立图层，在启动时窗口 vibrancy
        // 合成之前，该图层会显示满强度的 tint（亮/白线）。作为普通子控件，它绘入共享后备缓冲，与窗格其余部分
        // 叠加在同一层 vibrancy 上。
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
    }

    void setLine(const QColor& color, int inset) {
        if (m_color == color && m_inset == inset)
            return;
        m_color = color;
        m_inset = qMax(0, inset);
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        // Under a translucent top-level the backing store isn't auto-cleared on macOS, so a
        // translucent stroke would stack alpha every repaint and darken toward black; erase first
        // with Source (replace). On an opaque top-level Qt clears the (translucent) widget for us —
        // and a Source fill there would write RGBA(0,0,0,0) that renders black — so use the default
        // SourceOver over the already-cleared backing.
        // zh_CN: 半透明顶层下 macOS 后备缓冲不会自动清除，半透明描边会逐帧叠加 alpha、最终发黑，故先用 Source
        //（替换）擦除。不透明顶层下 Qt 会替我们清除（半透明）控件——此时 Source 会写入 RGBA(0,0,0,0) 渲染成黑，
        // 故改用默认的 SourceOver 在已清除的后备缓冲上绘制。
        const bool translucent = window() && window()->testAttribute(Qt::WA_TranslucentBackground);
        if (translucent) {
            painter.setCompositionMode(QPainter::CompositionMode_Source);
            painter.fillRect(rect(), Qt::transparent);
        }
        const QRect line = rect().adjusted(m_inset, 0, -m_inset, 0);
        if (line.width() > 0 && m_color.alpha() > 0) {
            painter.setCompositionMode(translucent ? QPainter::CompositionMode_Source
                                                   : QPainter::CompositionMode_SourceOver);
            painter.fillRect(line, m_color);
        }
    }

private:
    QColor m_color;
    int m_inset = 0;
};

} // namespace

GalleryNavigationPane::GalleryNavigationPane(const QVector<GalleryNavigationItem>& items, QWidget* parent)
    : QWidget(parent)
    , m_items(items)
{
    setObjectName(QStringLiteral("galleryNavigationPane"));
    setSizePolicy(QSizePolicy::Expanding, isFooterOnly() ? QSizePolicy::Fixed : QSizePolicy::Expanding);
    setMinimumHeight(isFooterOnly() ? kRouteHeight + 9 : 0);
    m_compactVisualAnimation = new QPropertyAnimation(this, "compactVisualProgress", this);
    connect(m_compactVisualAnimation, &QPropertyAnimation::finished,
            this, [this]() {
                setCompactVisualProgress(m_compact ? 1.0 : 0.0);
                updateCompactRowVisibility();
            });
    m_settingsIconRotationAnimation = new QPropertyAnimation(this, "settingsIconRotation", this);
    m_settingsIconRotationAnimation->setObjectName(QStringLiteral("gallerySettingsIconRotationAnimation"));
    connect(m_settingsIconRotationAnimation, &QPropertyAnimation::valueChanged,
            this, [this](const QVariant& value) {
                setSettingsIconRotation(value.toReal());
            });
    connect(m_settingsIconRotationAnimation, &QPropertyAnimation::finished,
            this, [this]() {
                setSettingsIconRotation(0.0);
            });
    rebuild();
}

QStringList GalleryNavigationPane::routeIds() const
{
    QStringList ids;
    for (const GalleryNavigationItem& item : m_items) {
        if (item.kind != GalleryNavigationItem::Kind::SectionHeader)
            ids.append(item.id);
    }
    return ids;
}

QStringList GalleryNavigationPane::visibleTitles() const
{
    QStringList titles;
    for (const GalleryNavigationItem& item : m_items)
        titles.append(item.title);
    return titles;
}

bool GalleryNavigationPane::containsRoute(const QString& routeId) const
{
    return m_routeIndexes.contains(routeId);
}

QModelIndex GalleryNavigationPane::indexForRouteId(const QString& routeId) const
{
    const auto iterator = m_routeIndexes.constFind(routeId);
    return iterator == m_routeIndexes.constEnd() ? QModelIndex() : QModelIndex(iterator.value());
}

void GalleryNavigationPane::setSelectedRouteId(const QString& routeId)
{
    if (m_selectedRouteId == routeId)
        return;
    LOG_DEBUG(QStringLiteral("GalleryNavigationPane selectedRouteChanged object=%1 old=%2 new=%3")
                  .arg(objectName(), m_selectedRouteId, routeId));
    m_selectedRouteId = routeId;
    updateButtonStyles();
    emit selectedRouteIdChanged(m_selectedRouteId);
}

void GalleryNavigationPane::setCompact(bool compact)
{
    if (m_compact == compact)
        return;

    m_compact = compact;
    hideCompactToolTip();
    LOG_DEBUG(QStringLiteral("GalleryNavigationPane compactChanged object=%1 compact=%2")
                  .arg(objectName(), compact ? QStringLiteral("true") : QStringLiteral("false")));
    syncCompactVisualProperties();
    if (compact && m_treeView)
        m_treeView->collapseAll();
    if (!compact)
        closeCompactFlyout();
    updateCompactRowVisibility();
    updateButtonStyles();
    startCompactVisualTransition(compact);
    emit compactChanged(m_compact);
}

void GalleryNavigationPane::setSurfaceVisible(bool visible)
{
    if (m_surfaceVisible == visible)
        return;

    m_surfaceVisible = visible;
    setAttribute(Qt::WA_NoSystemBackground, m_surfaceVisible);
    setAttribute(Qt::WA_TranslucentBackground, m_surfaceVisible);
    if (m_treeView) {
        m_treeView->setBackgroundVisible(false);
        m_treeView->setProperty("fluentPreserveParentSurface", m_surfaceVisible);
        if (m_treeView->viewport()) {
            m_treeView->viewport()->setProperty("fluentPreserveParentSurface", m_surfaceVisible);
            m_treeView->viewport()->setAttribute(Qt::WA_NoSystemBackground, m_surfaceVisible);
            m_treeView->viewport()->update();
        }
    }
    update();
}

void GalleryNavigationPane::setCompactVisualProgress(qreal progress)
{
    const qreal normalized = qBound<qreal>(0.0, progress, 1.0);
    if (qAbs(m_compactVisualProgress - normalized) <= 0.0001)
        return;

    const bool wasFullyCompact = m_compact && m_compactVisualProgress >= 0.999;
    m_compactVisualProgress = normalized;
    syncCompactVisualProperties();
    const bool isFullyCompact = m_compact && m_compactVisualProgress >= 0.999;
    if (wasFullyCompact != isFullyCompact || !m_compact)
        updateCompactRowVisibility();
    if (m_treeView) {
        m_treeView->doItemsLayout();
        if (m_treeView->viewport())
            m_treeView->viewport()->update();
    }
}

void GalleryNavigationPane::setSettingsIconRotation(qreal rotation)
{
    qreal normalized = std::fmod(rotation, kSettingsIconRotationDegrees);
    if (normalized < 0)
        normalized += kSettingsIconRotationDegrees;
    if (qAbs(m_settingsIconRotation - normalized) <= 0.0001)
        return;

    m_settingsIconRotation = normalized;
    if (m_treeView) {
        m_treeView->setProperty("gallerySettingsIconRotation", m_settingsIconRotation);
        if (m_treeView->viewport()) {
            m_treeView->viewport()->setProperty("gallerySettingsIconRotation", m_settingsIconRotation);
            m_treeView->viewport()->update();
        }
    }
}

void GalleryNavigationPane::onThemeUpdated()
{
    updateDividerPalette();
    update();
    if (m_treeView && m_treeView->viewport())
        m_treeView->viewport()->update();
}

void GalleryNavigationPane::paintEvent(QPaintEvent* event)
{
    QWidget::paintEvent(event);
}

bool GalleryNavigationPane::eventFilter(QObject* watched, QEvent* event)
{
    if (m_treeView && watched == m_treeView->viewport()) {
        switch (event->type()) {
        case QEvent::ToolTip: {
            const auto* helpEvent = static_cast<QHelpEvent*>(event);
            const QModelIndex index = m_treeView->indexAt(helpEvent->pos());
            if (m_compact && m_compactVisualProgress >= 0.999 && index.isValid())
                showCompactToolTip(index);
            else
                hideCompactToolTip();
            // Model rows carry tooltip text so Qt starts its usual hover timer. The Fluent
            // bubble owns presentation in compact mode; expanded mode intentionally has none.
            return true;
        }
        case QEvent::MouseMove:
            if (m_compactToolTip && m_compactToolTip->isVisible()) {
                const auto* mouseEvent = static_cast<QMouseEvent*>(event);
                if (m_treeView->indexAt(mouseEvent->pos()) != m_compactToolTipIndex)
                    hideCompactToolTip();
            }
            break;
        case QEvent::Leave:
        case QEvent::Hide:
        case QEvent::MouseButtonPress:
        case QEvent::Wheel:
        case QEvent::Resize:
            hideCompactToolTip();
            break;
        default:
            break;
        }
    }

    return QWidget::eventFilter(watched, event);
}

void GalleryNavigationPane::rebuild()
{
    m_routeIndexes.clear();

    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    if (isFooterOnly()) {
        m_footerDivider = new FooterDividerLine(this);
        m_footerDivider->setObjectName(QStringLiteral("galleryFooterNavigationDivider"));
        m_footerDivider->setFixedHeight(1);
        m_footerDivider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        outerLayout->addWidget(m_footerDivider);
        updateDividerPalette();
    }

    m_treeView = new fluent::collections::TreeView(this);
    m_treeView->setObjectName(isFooterOnly()
                                  ? QStringLiteral("galleryFooterNavigationTreeView")
                                  : QStringLiteral("galleryMainNavigationTreeView"));
    syncCompactVisualProperties();
    LOG_DEBUG(QStringLiteral("GalleryNavigationPane rebuild tree=%1 itemCount=%2 footerOnly=%3")
                  .arg(m_treeView->objectName())
                  .arg(m_items.size())
                  .arg(isFooterOnly() ? QStringLiteral("true") : QStringLiteral("false")));
    m_treeView->setBorderVisible(false);
    m_treeView->setBackgroundVisible(false);
    m_treeView->setProperty("fluentPreserveParentSurface", m_surfaceVisible);
    if (m_treeView->viewport()) {
        m_treeView->viewport()->setProperty("fluentPreserveParentSurface", m_surfaceVisible);
        m_treeView->viewport()->setAttribute(Qt::WA_NoSystemBackground, m_surfaceVisible);
        m_treeView->viewport()->installEventFilter(this);
    }
    m_treeView->setHorizontalFluentScrollBarEnabled(false);
    // Navigation chrome stops cleanly at the scroll edge — an elastic bounce reads as a glitch
    // here and would briefly trap the wheel at the boundary. zh_CN: 导航 chrome 在滚动边界干脆
    // 停住——此处的弹性回弹会显得突兀，且会在边界处短暂卡住滚轮。
    m_treeView->setOverscrollEnabled(false);
    m_treeView->setIndentation(0);
    m_treeView->setIndicatorMotionAnimationEnabled(true);
    m_treeView->setSelectionIndicatorVisible(true);
    fluent::collections::TreeView::SelectionIndicatorStyle indicatorStyle;
    indicatorStyle.inset = kRowLeftInset + 3.0;
    indicatorStyle.width = kSelectionIndicatorWidth;
    indicatorStyle.height = kSelectionIndicatorHeight;
    indicatorStyle.insetRole = IndicatorInsetRole;
    m_treeView->setSelectionIndicatorStyle(indicatorStyle);
    m_treeView->setSelectionMode(fluent::collections::TreeView::TreeSelectionMode::Single);
    m_treeView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    if (auto* scrollBar = m_treeView->verticalFluentScrollBar())
        scrollBar->setThickness(5);

    m_model = new QStandardItemModel(m_treeView);
    QHash<QString, QStandardItem*> categoryItems;
    for (const GalleryNavigationItem& item : m_items)
        appendNavigationItem(m_model, categoryItems, item);

    m_treeView->setModel(m_model);
    m_treeView->setItemDelegate(makeGalleryNavigationDelegate(this, m_treeView));
    if (!isFooterOnly())
        m_treeView->collapseAll();
    updateCompactRowVisibility();

    connect(m_treeView, &fluent::collections::TreeView::itemPressed,
            this, [this](const QModelIndex& index) {
                const QString routeId = index.data(RouteIdRole).toString();
                if (routeId.isEmpty()) {
                    LOG_TRACE(QStringLiteral("GalleryNavigationPane itemPressed tree=%1 state=ignored reason=empty-route")
                                  .arg(m_treeView ? m_treeView->objectName() : objectName()));
                    return;
                }
                const bool hasChildren = m_model && m_model->hasChildren(index);
                LOG_DEBUG(QStringLiteral("GalleryNavigationPane itemPressed tree=%1 routeId=%2 hasChildren=%3")
                              .arg(m_treeView ? m_treeView->objectName() : objectName(),
                                   routeId,
                                   hasChildren ? QStringLiteral("true") : QStringLiteral("false")));
                if (routeId == QStringLiteral("settings"))
                    startSettingsIconRotation();
                setSelectedRouteId(routeId);
                if (m_compact && hasChildren) {
                    showCompactFlyoutForIndex(index);
                } else if (m_treeView && hasChildren) {
                    m_treeView->toggleExpanded(index);
                } else if (m_compact) {
                    closeCompactFlyout();
                }
                emit routeActivated(routeId);
            });

    outerLayout->addWidget(m_treeView);
    updateButtonStyles();
    LOG_DEBUG(QStringLiteral("GalleryNavigationPane modelReady tree=%1 routes=%2 footerOnly=%3")
                  .arg(m_treeView->objectName())
                  .arg(m_routeIndexes.size())
                  .arg(isFooterOnly() ? QStringLiteral("true") : QStringLiteral("false")));
}

void GalleryNavigationPane::updateButtonStyles()
{
    if (!m_treeView || !m_treeView->selectionModel())
        return;

    const QModelIndex index = indexForRouteId(m_selectedRouteId);
    if (!index.isValid()) {
        m_treeView->clearSelection();
        m_treeView->setCurrentIndex(QModelIndex());
        LOG_TRACE(QStringLiteral("GalleryNavigationPane selectionCleared tree=%1 routeId=%2 reason=missing-index")
                      .arg(m_treeView->objectName(), m_selectedRouteId));
        return;
    }

    if (!m_compact) {
        QModelIndex parentIndex = index.parent();
        while (parentIndex.isValid()) {
            m_treeView->expand(parentIndex);
            parentIndex = parentIndex.parent();
        }
    }

    const QModelIndex visualIndex = visualSelectionIndex(index);
    auto* verticalFluentBar = m_treeView->verticalFluentScrollBar();
    auto* horizontalFluentBar = m_treeView->horizontalFluentScrollBar();
    const bool verticalSignalsBlocked = verticalFluentBar && verticalFluentBar->signalsBlocked();
    const bool horizontalSignalsBlocked = horizontalFluentBar && horizontalFluentBar->signalsBlocked();
    if (verticalFluentBar)
        verticalFluentBar->blockSignals(true);
    if (horizontalFluentBar)
        horizontalFluentBar->blockSignals(true);
    m_treeView->setSelectedItem(visualIndex);
    m_treeView->scrollTo(visualIndex, QAbstractItemView::EnsureVisible);
    if (verticalFluentBar)
        verticalFluentBar->blockSignals(verticalSignalsBlocked);
    if (horizontalFluentBar)
        horizontalFluentBar->blockSignals(horizontalSignalsBlocked);
    LOG_TRACE(QStringLiteral("GalleryNavigationPane selectionApplied tree=%1 routeId=%2")
                  .arg(m_treeView->objectName(), m_selectedRouteId));
}

void GalleryNavigationPane::updateCompactRowVisibility()
{
    if (!m_treeView || !m_model)
        return;

    const bool hideSectionHeaders = m_compact && m_compactVisualProgress >= 0.999;
    for (int row = 0; row < m_model->rowCount(); ++row) {
        const QModelIndex index = m_model->index(row, 0);
        const auto kind = static_cast<GalleryNavigationItem::Kind>(index.data(KindRole).toInt());
        m_treeView->setRowHidden(row, QModelIndex(),
                                 hideSectionHeaders && kind == GalleryNavigationItem::Kind::SectionHeader);
    }
}

void GalleryNavigationPane::syncCompactVisualProperties()
{
    if (!m_treeView)
        return;

    m_treeView->setProperty("galleryCompact", m_compact);
    m_treeView->setProperty("galleryCompactVisualProgress", m_compactVisualProgress);
    m_treeView->setProperty("gallerySettingsIconRotation", m_settingsIconRotation);
    if (m_treeView->viewport()) {
        m_treeView->viewport()->setProperty("galleryCompact", m_compact);
        m_treeView->viewport()->setProperty("galleryCompactVisualProgress", m_compactVisualProgress);
        m_treeView->viewport()->setProperty("gallerySettingsIconRotation", m_settingsIconRotation);
    }
}

void GalleryNavigationPane::updateDividerPalette()
{
    if (!m_footerDivider)
        return;

    // Inset the separator so it reads as a refined, centered hairline rather than a hard
    // edge-to-edge bar — the latter looks crude against the clean Mica/vibrancy chrome (and an
    // inset rule is the native macOS convention). The stroke is softened to ~40% alpha so it
    // whispers the separation instead of drawing a hard gray line over the translucent pane.
    // It's drawn by FooterDividerLine (CompositionMode_Source) so the translucent stroke doesn't
    // accumulate/darken across repaints under the translucent backdrop.
    // zh_CN: 让分隔线内缩，呈现精致、居中的细线，而非生硬的整宽横条（在干净的 Mica/vibrancy chrome 上更协调，
    // 内缩也符合 macOS 原生习惯）。描边淡化到约 40% alpha，在半透明窗格上轻声示意分隔。由 FooterDividerLine
    //（CompositionMode_Source）绘制，故半透明描边不会在半透明背景下逐帧叠加发黑。
    const auto colors = themeColors();
    QColor divider = colors.strokeDivider;
    divider.setAlphaF(divider.alphaF() * 0.4);
    // m_footerDivider is always a FooterDividerLine (created in the footer-only branch) and is
    // non-null here (guarded above). zh_CN: m_footerDivider 必为 FooterDividerLine（页脚分支创建）且此处非空。
    static_cast<FooterDividerLine*>(m_footerDivider)->setLine(divider, 16);
}

void GalleryNavigationPane::startCompactVisualTransition(bool compact)
{
    const qreal endValue = compact ? 1.0 : 0.0;
    if (!m_compactVisualAnimation || qAbs(m_compactVisualProgress - endValue) <= 0.0001) {
        setCompactVisualProgress(endValue);
        updateCompactRowVisibility();
        return;
    }

    if (!isVisible() || !window() || !window()->isVisible()) {
        if (m_compactVisualAnimation)
            m_compactVisualAnimation->stop();
        setCompactVisualProgress(endValue);
        updateCompactRowVisibility();
        return;
    }

    const auto animation = themeAnimation();
    m_compactVisualAnimation->stop();
    m_compactVisualAnimation->setDuration(animation.normal);
    m_compactVisualAnimation->setEasingCurve(animation.decelerate);
    m_compactVisualAnimation->setStartValue(m_compactVisualProgress);
    m_compactVisualAnimation->setEndValue(endValue);
    m_compactVisualAnimation->start();
}

void GalleryNavigationPane::startSettingsIconRotation()
{
    if (!m_settingsIconRotationAnimation)
        return;

    if (!isVisible() || !window() || !window()->isVisible()) {
        setSettingsIconRotation(0.0);
        return;
    }

    m_settingsIconRotationAnimation->stop();
    const auto animation = themeAnimation();
    m_settingsIconRotationAnimation->setDuration(animation.slow);
    m_settingsIconRotationAnimation->setEasingCurve(animation.decelerate);
    m_settingsIconRotationAnimation->setStartValue(m_settingsIconRotation);
    m_settingsIconRotationAnimation->setEndValue(m_settingsIconRotation + kSettingsIconRotationDegrees - 0.01);
    m_settingsIconRotationAnimation->start();
}

QModelIndex GalleryNavigationPane::visualSelectionIndex(const QModelIndex& routeIndex) const
{
    if (!m_compact || !routeIndex.isValid())
        return routeIndex;

    const QModelIndex parentIndex = routeIndex.parent();
    return parentIndex.isValid() ? parentIndex : routeIndex;
}

void GalleryNavigationPane::showCompactFlyoutForIndex(const QModelIndex& index)
{
    if (!m_compact || !m_treeView || !m_model || !index.isValid() || !m_model->hasChildren(index))
        return;

    closeCompactFlyout(false);

    const QRect visualRect = m_treeView->visualRect(index);
    if (visualRect.isEmpty())
        return;

    if (!m_compactFlyoutAnchor) {
        m_compactFlyoutAnchor = new QWidget(m_treeView->viewport());
        m_compactFlyoutAnchor->setObjectName(QStringLiteral("galleryCompactNavigationFlyoutAnchor"));
        m_compactFlyoutAnchor->setAttribute(Qt::WA_TransparentForMouseEvents);
    }
    m_compactFlyoutAnchor->setGeometry(0,
                                       visualRect.top(),
                                       kCompactPaneWidth,
                                       visualRect.height());
    m_compactFlyoutAnchor->show();

    m_compactFlyout = new fluent::dialogs_flyouts::Popup(this);
    m_compactFlyout->setObjectName(QStringLiteral("galleryCompactNavigationFlyout"));
    m_compactFlyout->setAnimationEnabled(true);
    m_compactFlyout->setClosePolicy(fluent::dialogs_flyouts::Popup::ClosePolicy(
        fluent::dialogs_flyouts::Popup::CloseOnPressOutside |
        fluent::dialogs_flyouts::Popup::CloseOnEscape));

    m_compactFlyoutPanel = new CompactFlyoutPanel(m_compactFlyout);
    m_compactFlyoutPanel->setObjectName(QStringLiteral("galleryCompactNavigationFlyoutPanel"));
    auto* layout = new QVBoxLayout(m_compactFlyoutPanel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    for (int row = 0; row < m_model->rowCount(index); ++row) {
        const QModelIndex childIndex = m_model->index(row, 0, index);
        const QString routeId = childIndex.data(RouteIdRole).toString();
        if (routeId.isEmpty())
            continue;
        auto* itemRow = new CompactFlyoutRow(routeId,
                                             childIndex.data(Qt::DisplayRole).toString(),
                                             routeId == m_selectedRouteId,
                                             m_compactFlyoutPanel);
        itemRow->onActivated = [this](const QString& activatedRouteId) {
            setSelectedRouteId(activatedRouteId);
            emit routeActivated(activatedRouteId);
            QTimer::singleShot(0, this, [this]() {
                closeCompactFlyout();
            });
        };
        layout->addWidget(itemRow);
    }

    const QSize contentSize = m_compactFlyoutPanel->sizeHint();
    QWidget* topLevel = m_treeView->window();
    const QPoint anchorTopLeft = m_compactFlyoutAnchor->mapTo(topLevel, QPoint(0, 0));
    const int safeTop = qMax(kCompactFlyoutWindowMargin,
                             m_treeView->mapTo(topLevel, QPoint(0, 0)).y() + kCompactFlyoutWindowMargin);
    const int safeBottom = topLevel->height() - kCompactFlyoutWindowMargin;
    const int maxVisibleHeight = qMax(kRouteHeight, safeBottom - safeTop);
    const QSize cardSize(contentSize.width() + kCompactFlyoutContentMargins.left() + kCompactFlyoutContentMargins.right(),
                         qMin(contentSize.height() + kCompactFlyoutContentMargins.top() + kCompactFlyoutContentMargins.bottom(),
                              maxVisibleHeight));
    const int preferredTop = anchorTopLeft.y() + kCompactFlyoutVerticalOffset;
    const int cardTop = qBound(safeTop,
                               preferredTop,
                               qMax(safeTop, safeBottom - cardSize.height()));
    const int cardLeft = anchorTopLeft.x() + m_compactFlyoutAnchor->width() + kCompactFlyoutHorizontalOffset;

    const QRect contentRect = QRect(QPoint(0, 0), cardSize).marginsRemoved(kCompactFlyoutContentMargins);
    m_compactFlyoutPanel->resize(QSize(contentRect.width(), contentSize.height()));
    m_compactFlyout->resize(fluent::overlay::outerSizeForVisibleCard(cardSize));
    m_compactFlyoutPanel->setGeometry(fluent::overlay::visibleCardRect(m_compactFlyout->rect())
                                          .marginsRemoved(kCompactFlyoutContentMargins));
    m_compactFlyoutPanel->show();
    m_compactFlyout->setPosition(topLevel, QPoint(cardLeft, cardTop));
    m_compactFlyout->open();

    const QPoint endPos = m_compactFlyout->pos();
    auto* positionAnimation = new QPropertyAnimation(m_compactFlyout, "pos", m_compactFlyout);
    positionAnimation->setDuration(themeAnimation().fast);
    positionAnimation->setEasingCurve(themeAnimation().decelerate);
    positionAnimation->setStartValue(endPos - QPoint(kCompactFlyoutEntranceOffset, 0));
    positionAnimation->setEndValue(endPos);
    m_compactFlyout->move(positionAnimation->startValue().toPoint());
    positionAnimation->start(QAbstractAnimation::DeleteWhenStopped);

    LOG_DEBUG(QStringLiteral("GalleryNavigationPane compactFlyoutOpened parentRouteId=%1 childCount=%2")
                  .arg(index.data(RouteIdRole).toString())
                  .arg(m_model->rowCount(index)));
}

void GalleryNavigationPane::showCompactToolTip(const QModelIndex& index)
{
    if (!m_compact || m_compactVisualProgress < 0.999 || !m_treeView || !index.isValid())
        return;

    const auto kind = static_cast<GalleryNavigationItem::Kind>(index.data(KindRole).toInt());
    const QString text = index.data(Qt::ToolTipRole).toString();
    const QRect rowRect = m_treeView->visualRect(index);
    if (kind == GalleryNavigationItem::Kind::SectionHeader || text.isEmpty()
        || rowRect.isEmpty() || !m_treeView->viewport()->rect().intersects(rowRect)) {
        hideCompactToolTip();
        return;
    }

    if (!m_compactToolTip) {
        m_compactToolTip = new fluent::status_info::ToolTip(this);
        m_compactToolTip->setObjectName(QStringLiteral("galleryCompactNavigationToolTip"));
        m_compactToolTip->setAnimationEnabled(true);
    }
    m_compactToolTip->setText(text);
    m_compactToolTipIndex = index;

    // Center the visible bubble above the compact row. ToolTip reserves a transparent shadow
    // band, so offset that band out of the measured gap.
    const int shadow = m_compactToolTip->shadowMargin();
    const QPoint anchor = m_treeView->viewport()->mapToGlobal(
        QPoint(rowRect.center().x(), rowRect.top()));
    const int x = anchor.x() - m_compactToolTip->width() / 2;
    const int y = anchor.y() - kCompactToolTipGap - m_compactToolTip->height() + shadow;
    m_compactToolTip->move(x, y);
    m_compactToolTip->show();
    m_compactToolTip->raise();
}

void GalleryNavigationPane::hideCompactToolTip()
{
    m_compactToolTipIndex = QPersistentModelIndex();
    if (m_compactToolTip)
        m_compactToolTip->hide();
}

void GalleryNavigationPane::closeCompactFlyout(bool animated)
{
    if (m_compactFlyout) {
        auto* popup = m_compactFlyout;
        m_compactFlyout = nullptr;
        m_compactFlyoutPanel = nullptr;
        if (animated && popup->isVisible()) {
            connect(popup, &fluent::dialogs_flyouts::Popup::closed,
                    popup, &QObject::deleteLater);
            popup->close();
        } else {
            popup->hide();
            popup->deleteLater();
        }
    }
    if (m_compactFlyoutAnchor)
        m_compactFlyoutAnchor->hide();
}

bool GalleryNavigationPane::isFooterOnly() const
{
    return m_items.size() == 1 && m_items.first().kind == GalleryNavigationItem::Kind::FooterRoute;
}

QStandardItem* GalleryNavigationPane::createItem(const GalleryNavigationItem& item)
{
    auto* standardItem = new QStandardItem(item.title);
    standardItem->setEditable(false);
    standardItem->setData(item.id, RouteIdRole);
    standardItem->setData(static_cast<int>(item.kind), KindRole);
    standardItem->setData(item.parentId, ParentRouteIdRole);
    standardItem->setData(item.kind == GalleryNavigationItem::Kind::SectionHeader
                              ? QString()
                              : item.title,
                          Qt::ToolTipRole);
    standardItem->setData(item.kind == GalleryNavigationItem::Kind::ComponentRoute ? QString() : item.iconGlyph,
                          IconGlyphRole);
    if (item.kind == GalleryNavigationItem::Kind::ComponentRoute)
        standardItem->setData(kRowLeftInset + kTextStart
                                  - kSelectionIndicatorWidth
                                  - kSelectionIndicatorTextGap,
                              IndicatorInsetRole);
    standardItem->setFlags(item.kind == GalleryNavigationItem::Kind::SectionHeader
                               ? Qt::ItemIsEnabled
                               : Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    return standardItem;
}

void GalleryNavigationPane::appendNavigationItem(QStandardItemModel* model,
                                                 QHash<QString, QStandardItem*>& categoryItems,
                                                 const GalleryNavigationItem& item)
{
    QStandardItem* standardItem = createItem(item);
    if (item.kind == GalleryNavigationItem::Kind::ComponentRoute && categoryItems.contains(item.parentId)) {
        categoryItems.value(item.parentId)->appendRow(standardItem);
    } else {
        model->appendRow(standardItem);
    }

    if (item.kind == GalleryNavigationItem::Kind::CategoryRoute)
        categoryItems.insert(item.id, standardItem);
    if (item.kind != GalleryNavigationItem::Kind::SectionHeader && !item.id.isEmpty())
        m_routeIndexes.insert(item.id, QPersistentModelIndex(standardItem->index()));
}

} // namespace fluent::gallery

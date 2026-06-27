#include "ListView.h"

#include <algorithm>

#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QApplication>
#include <QDateTime>
#include <QItemSelectionModel>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSet>
#include <QShowEvent>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QVariantAnimation>
#include <QWheelEvent>

#include "compatibility/QtCompat.h"
#include "design/CornerRadius.h"
#include "design/Spacing.h"
#include "design/Typography.h"
#include "components/scrolling/OverlayScrollChrome.h"
#include "components/scrolling/OverscrollController.h"
#include "components/scrolling/ScrollBar.h"

namespace fluent::collections {

namespace {
// Pixels scrolled per mouse-wheel notch (delta 120); the cross-platform wheel/overscroll state
// machine now lives in fluent::scrolling::OverscrollController.
// zh_CN: 每个滚轮刻度（delta 120）滚动的像素数；跨平台滚轮/回弹状态机现位于 OverscrollController。
constexpr qreal kDiscreteWheelStepPx = ::Spacing::ControlHeight::Large;
constexpr int   kScrollBarEdgeInset = ::Spacing::XSmall / 2;

qreal lerp(qreal from, qreal to, qreal progress) {
    return from + (to - from) * progress;
}

// Color a child QLabel through its OWN style sheet rather than its palette. A palette WindowText color
// is silently dropped whenever an ancestor sets a style sheet (Qt installs QStyleSheetStyle over the
// subtree and ignores child palettes) — e.g. the gallery sample card, where the header/footer text then
// renders near-black in dark theme. A style-sheet color always wins. zh_CN: 用 label 自身样式表上色而非
// palette。任何祖先设置样式表时(安装 QStyleSheetStyle 并忽略子 palette),palette 的 WindowText 会被丢弃
// ——如画廊示例卡,header/footer 文案在深色主题里变近黑。样式表颜色始终生效。
QString cssRgba(const QColor& c) {
    return QStringLiteral("rgba(%1, %2, %3, %4)")
        .arg(c.red()).arg(c.green()).arg(c.blue()).arg(c.alpha());
}

qreal indicatorLeadingProgress(qreal progress) {
    return qBound(0.0, progress * 1.35, 1.0);
}

qreal indicatorTrailingProgress(qreal progress) {
    return qBound(0.0, (progress - 0.18) / 0.82, 1.0);
}
} // namespace

// ── Section proxy delegate ────────────────────────────────────────────────────
// Wraps the user's delegate and adds extra space + section header painting
// for the first row of each section group.

class SectionProxyDelegate : public QStyledItemDelegate {
public:
    SectionProxyDelegate(ListView* listView, QObject* parent = nullptr)
        : QStyledItemDelegate(parent), m_listView(listView) {}

    void setInnerDelegate(QAbstractItemDelegate* d) { m_inner = d; }
    QAbstractItemDelegate* innerDelegate() const { return m_inner; }

    int sectionHeaderHeight() const {
        const QFont titleFont = m_listView->themeFont(Typography::FontRole::Title).toQFont();
        const QFontMetrics fm(titleFont);
        return fm.height() + 4; // text + separator(1px) + padding(3px)
    }

    bool isSectionStart(int row) const {
        if (!m_listView->sectionEnabled() || !m_listView->m_sectionKeyFunc) return false;
        if (row == 0) return true;
        auto* model = m_listView->model();
        if (!model || row >= model->rowCount()) return false;
        return m_listView->m_sectionKeyFunc(row) != m_listView->m_sectionKeyFunc(row - 1);
    }

    QString sectionKey(int row) const {
        if (!m_listView->m_sectionKeyFunc) return {};
        return m_listView->m_sectionKeyFunc(row);
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QSize s = m_inner ? m_inner->sizeHint(option, index)
                          : QStyledItemDelegate::sizeHint(option, index);
        if (isSectionStart(index.row())) {
            s.rheight() += sectionHeaderHeight();
        }
        return s;
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override {
        const int row = index.row();
        const bool isStart = isSectionStart(row);
        const int headerH = isStart ? sectionHeaderHeight() : 0;

        if (isStart) {
            const auto& c = m_listView->themeColors();
            const QFont titleFont = m_listView->themeFont(Typography::FontRole::Title).toQFont();
            const QFontMetrics titleFm(titleFont);
            const int hPad = ::Spacing::Padding::ListItemHorizontal;
            const int textH = titleFm.height();

            // Fill the section-header area to clear any hover artifacts — but only when the container
            // paints its own surface. With the surface hidden (e.g. the gallery flat previews, where
            // the list sits directly on the sample panel) the viewport is transparent and the rows let
            // the parent panel show through; a bgLayer fill would read as a darker nested block. The
            // viewport's backing store has no alpha, so CompositionMode_Clear would write solid black —
            // instead we paint nothing and let the section title sit on the already-composited parent
            // panel like the rows do. zh_CN: 填充分组头区域以清除 hover 残影——但仅在容器绘制自身表面时。
            // 表面隐藏时(如画廊扁平预览,列表直接坐在示例面板上),viewport 透明、行让父面板透出;填 bgLayer
            // 会变成更暗的嵌套块,而 viewport backing store 无 alpha,CompositionMode_Clear 会写成纯黑——
            // 故此时不绘制任何底色,让分组标题像普通行一样坐在已合成的父面板上。
            QRect headerArea(option.rect.left(), option.rect.top(),
                             option.rect.width(), headerH);
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing);
            if (m_listView->isBackgroundVisible()) {
                painter->fillRect(headerArea, c.bgLayer);
            }

            // Section title text
            QRect textRect(option.rect.left() + hPad, option.rect.top(),
                           option.rect.width() - 2 * hPad, textH);
            painter->setFont(titleFont);
            painter->setPen(c.textPrimary);
            painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, sectionKey(row));

            // Separator line below text
            const int lineY = option.rect.top() + textH + 2;
            painter->setPen(QPen(c.strokeDefault, 1.0));
            painter->drawLine(option.rect.left() + hPad, lineY,
                              option.rect.right() - hPad, lineY);

            painter->restore();
        }

        // Adjust rect for inner delegate (item content area only)
        QStyleOptionViewItem adjusted = option;
        adjusted.rect.setTop(option.rect.top() + headerH);

        // If mouse is actually in the section header zone, strip hover/press states
        // so the item below doesn't show false hover highlight
        if (isStart && (option.state & QStyle::State_MouseOver)) {
            QPoint mousePos = m_listView->viewport()->mapFromGlobal(QCursor::pos());
            if (mousePos.y() < option.rect.top() + headerH) {
                adjusted.state &= ~QStyle::State_MouseOver;
                adjusted.state &= ~QStyle::State_Sunken;
            }
        }

        if (m_inner) {
            m_inner->paint(painter, adjusted, index);
        } else {
            QStyledItemDelegate::paint(painter, adjusted, index);
        }
    }

private:
    ListView* m_listView = nullptr;
    QAbstractItemDelegate* m_inner = nullptr;
};

ListView::ListView(QWidget* parent)
    : QListView(parent) {

    m_fontRole = Typography::FontRole::Body;

    setFrameStyle(QFrame::NoFrame);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setMouseTracking(true);
    setSpacing(2);  // 2px around each item, also spacing first/last from the border. zh_CN: item 四周留 2px，兼顾首尾与边框的间隙。

    QListView::setSelectionMode(QAbstractItemView::SingleSelection);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    connect(this, &QAbstractItemView::clicked, this, [this](const QModelIndex& idx) {
        emit itemClicked(idx.row());
    });

    // Header/footer start empty; setHeader()/setHeaderText() create them on demand.
    // zh_CN: Header/footer 初始为空 —— 由 setHeader() / setHeaderText() 按需创建。

    // --- Fluent scroll bars ---
    m_vScrollBar = ::fluent::scrolling::createOverlayScrollBar(
        Qt::Vertical, this, verticalScrollBar(),
        QStringLiteral("fluentListViewScrollBar"));
    connect(verticalScrollBar(), &QScrollBar::rangeChanged,
            this, &ListView::syncFluentScrollBar);

    m_hScrollBar = ::fluent::scrolling::createOverlayScrollBar(
        Qt::Horizontal, this, horizontalScrollBar(),
        QStringLiteral("fluentListViewHScrollBar"));
    connect(horizontalScrollBar(), &QScrollBar::rangeChanged,
            this, &ListView::syncFluentHScrollBar);

    // --- Overscroll bounce (shared controller) ---
    fluent::scrolling::OverscrollController::Hooks hooks;
    hooks.isHorizontal = [this] { return flow() == LeftToRight; };
    hooks.scrollBar = [this] {
        return flow() == LeftToRight ? horizontalScrollBar() : verticalScrollBar();
    };
    hooks.normalScroll = [this](qreal scrollPx) {
        QScrollBar* sb = flow() == LeftToRight ? horizontalScrollBar() : verticalScrollBar();
        const int before = sb->value();
        sb->setValue(before - qRound(scrollPx));
        return sb->value() != before;
    };
    hooks.onOverscrollChanged = [this] {
        if (viewport())
            viewport()->update();
        syncFluentScrollBar();
        syncFluentHScrollBar();
    };
    hooks.fallbackWheel = [this](QWheelEvent* e) { QListView::wheelEvent(e); };
    m_overscroll = new fluent::scrolling::OverscrollController(
        viewport(), kDiscreteWheelStepPx, std::move(hooks), this);

    // --- Selected indicator motion ---
    m_selectedIndicatorAnimation = new QVariantAnimation(this);
    m_selectedIndicatorAnimation->setDuration(themeAnimation().normal);
    m_selectedIndicatorAnimation->setEasingCurve(themeAnimation().decelerate);
    m_selectedIndicatorAnimation->setStartValue(0.0);
    m_selectedIndicatorAnimation->setEndValue(1.0);
    connect(m_selectedIndicatorAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        setSelectedIndicatorProgress(value.toReal());
        if (viewport())
            viewport()->update();
    });
    connect(m_selectedIndicatorAnimation, &QVariantAnimation::finished, this, [this]() {
        setSelectedIndicatorProgress(1.0);
        if (viewport())
            viewport()->update();
    });

    syncFluentScrollBar();
    syncFluentHScrollBar();
    onThemeUpdated();
}

ListView::~ListView() {
    // Stop animations before destruction so no pending tick fires on a half-destroyed object.
    // The overscroll controller owns its own bounce anim/timer and stops them as a child.
    // zh_CN: 析构前停止动画，避免半销毁对象上仍有回调；overscroll 控制器作为子对象自行停止其回弹动画/定时器。
    if (m_selectedIndicatorAnimation) m_selectedIndicatorAnimation->stop();
    clearMultiSelectedIndicatorState();
}

void ListView::setModel(QAbstractItemModel* model) {
    disconnectSelectedIndicatorModel();
    clearSelectedIndicatorState();
    QListView::setModel(model);
    connectSelectedIndicatorModel(model);
    if (usesMovingSelectedIndicator())
        updateSelectedIndicatorFromSelection();
}

void ListView::setSelectionModel(QItemSelectionModel* selectionModel) {
    clearSelectedIndicatorState();
    QListView::setSelectionModel(selectionModel);
    if (usesMovingSelectedIndicator())
        updateSelectedIndicatorFromSelection();
}

// ── Selection mode ────────────────────────────────────────────────────────────

void ListView::setSelectionMode(ListSelectionMode mode) {
    if (m_selectionMode == mode) return;
    m_selectionMode = mode;

    switch (mode) {
    case ListSelectionMode::None:
        QListView::setSelectionMode(QAbstractItemView::NoSelection);
        break;
    case ListSelectionMode::Single:
        QListView::setSelectionMode(QAbstractItemView::SingleSelection);
        break;
    case ListSelectionMode::Multiple:
        QListView::setSelectionMode(QAbstractItemView::MultiSelection);
        break;
    case ListSelectionMode::Extended:
        QListView::setSelectionMode(QAbstractItemView::ExtendedSelection);
        break;
    }
    clearSelectedIndicatorState();
    if (usesMovingSelectedIndicator())
        updateSelectedIndicatorFromSelection();
    else if (usesRevealSelectedIndicators() && selectionModel())
        syncMultiSelectedIndicators(QItemSelection(), QItemSelection());
    emit selectionModeChanged();
}

// ── Flow ──────────────────────────────────────────────────────────────────────

ListView::Flow ListView::flow() const {
    return QListView::flow();
}

void ListView::setFlow(Flow f) {
    if (QListView::flow() == f) return;
    // The scroll axis is changing; drop any active overscroll on the old axis.
    // zh_CN: 滚动轴即将改变，清掉旧轴上的 overscroll。
    if (m_overscroll) m_overscroll->cancel();
    QListView::setFlow(f);
    // Horizontal flow: scrollbar values must be in pixels so wheelEvent pixelDelta maps correctly.
    // Default ScrollPerItem makes scrollbar unit = item index, causing huge jumps.
    if (f == LeftToRight) {
        setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    }
    if (usesMovingSelectedIndicator())
        refreshSelectedIndicatorGeometry(true);
    syncFluentScrollBar();
    syncFluentHScrollBar();
    emit flowChanged();
}

// ── Appearance properties ────────────────────────────────────────────────────

void ListView::setFontRole(const QString& role) {
    if (m_fontRole == role) return;
    m_fontRole = role;
    applyThemeStyle();
    emit fontRoleChanged();
}

void ListView::setBorderVisible(bool visible) {
    if (m_borderVisible == visible) return;
    m_borderVisible = visible;
    update();
    emit borderVisibleChanged();
}

void ListView::setBackgroundVisible(bool visible) {
    if (m_backgroundVisible == visible) return;
    m_backgroundVisible = visible;
    if (viewport()) viewport()->update();
    emit backgroundVisibleChanged();
}

void ListView::setHeader(QWidget* widget) {
    if (m_header == widget) return;

    // Tear down the old header. zh_CN: 清理旧 header。
    if (m_header) {
        m_header->hide();
        if (m_ownsHeader) {
            m_header->deleteLater();
        }
    }
    m_header = widget;
    m_ownsHeader = false;

    if (m_header) {
        m_header->setParent(this);
        m_header->show();
    }
    updateViewportMargins();
    layoutHeader();
    layoutFooter();
    syncFluentScrollBar();
    emit headerChanged();
}

void ListView::setFooter(QWidget* widget) {
    if (m_footer == widget) return;

    // Tear down the old footer. zh_CN: 清理旧 footer。
    if (m_footer) {
        m_footer->hide();
        if (m_ownsFooter) {
            m_footer->deleteLater();
        }
    }
    m_footer = widget;
    m_ownsFooter = false;

    if (m_footer) {
        m_footer->setParent(this);
        m_footer->show();
    }
    updateViewportMargins();
    layoutHeader();
    layoutFooter();
    syncFluentScrollBar();
    emit footerChanged();
}

void ListView::setHeaderText(const QString& text) {
    if (m_headerText == text) return;
    m_headerText = text;

    if (text.isEmpty()) {
        // Clearing the convenience text removes the internal label.
        // zh_CN: 清空便捷文本 → 移除内部 label。
        if (m_ownsHeader) {
            setHeader(nullptr);
        }
    } else {
        // Reuse the existing label or create one. zh_CN: 复用已有 label 或新建。
        auto* lbl = m_ownsHeader ? qobject_cast<QLabel*>(m_header) : nullptr;
        if (!lbl) {
            lbl = new QLabel(this);
            lbl->setObjectName(QStringLiteral("fluentListViewHeader"));
            lbl->setIndent(::Spacing::Padding::ListItemHorizontal);
            setHeader(lbl);
            m_ownsHeader = true;
        }
        lbl->setText(text);
        applyThemeStyle();    // Fonts must be set for an accurate sizeHint. zh_CN: 确保字体已设置，sizeHint 准确。
    }
    emit headerTextChanged();
}

void ListView::setFooterText(const QString& text) {
    if (m_footerText == text) return;
    m_footerText = text;

    if (text.isEmpty()) {
        if (m_ownsFooter) {
            setFooter(nullptr);
        }
    } else {
        auto* lbl = m_ownsFooter ? qobject_cast<QLabel*>(m_footer) : nullptr;
        if (!lbl) {
            lbl = new QLabel(this);
            lbl->setObjectName(QStringLiteral("fluentListViewFooter"));
            lbl->setIndent(::Spacing::Padding::ListItemHorizontal);
            setFooter(lbl);
            m_ownsFooter = true;
        }
        lbl->setText(text);
        applyThemeStyle();
    }
    emit footerTextChanged();
}

void ListView::setPlaceholderText(const QString& text) {
    if (m_placeholderText == text) return;
    m_placeholderText = text;
    if (viewport()) viewport()->update();
    emit placeholderTextChanged();
}

// ── Drag reorder ──────────────────────────────────────────────────────────────

void ListView::setCanReorderItems(bool enabled) {
    if (m_canReorderItems == enabled) return;
    m_canReorderItems = enabled;
    emit canReorderItemsChanged();
}

bool ListView::isScrollChainingEnabled() const { return m_overscroll->isScrollChainingEnabled(); }

void ListView::setScrollChainingEnabled(bool enabled) {
    if (m_overscroll->isScrollChainingEnabled() == enabled) return;
    m_overscroll->setScrollChainingEnabled(enabled);
    emit scrollChainingEnabledChanged();
}

// ── Section ───────────────────────────────────────────────────────────────────

void ListView::setSectionEnabled(bool enabled) {
    if (m_sectionEnabled == enabled) return;
    m_sectionEnabled = enabled;
    installSectionProxy();
    if (viewport()) viewport()->update();
    emit sectionEnabledChanged();
}

void ListView::setSectionKeyFunction(SectionKeyFunc func) {
    m_sectionKeyFunc = std::move(func);
    installSectionProxy();
    if (m_sectionEnabled && viewport()) viewport()->update();
}

void ListView::installSectionProxy() {
    const bool need = m_sectionEnabled && m_sectionKeyFunc;

    if (need) {
        if (!m_sectionProxy) {
            // Capture the user's current delegate before wrapping
            m_userDelegate = itemDelegate();
            auto* proxy = new SectionProxyDelegate(this, this);
            proxy->setInnerDelegate(m_userDelegate);
            m_sectionProxy = proxy;
            QAbstractItemView::setItemDelegate(proxy);
        }
    } else if (m_sectionProxy) {
        // Restore the user's original delegate
        if (m_userDelegate) {
            QAbstractItemView::setItemDelegate(m_userDelegate);
        }
        m_sectionProxy->deleteLater();
        m_sectionProxy = nullptr;
        m_userDelegate = nullptr;
    }
}

// ── Selection API ─────────────────────────────────────────────────────────────

int ListView::selectedIndex() const {
    if (!selectionModel())
        return -1;
    auto idxList = selectionModel()->selectedIndexes();
    return idxList.isEmpty() ? -1 : idxList.first().row();
}

QList<int> ListView::selectedRows() const {
    QSet<int> seen;
    if (!selectionModel())
        return {};
    for (const auto& idx : selectionModel()->selectedIndexes())
        seen.insert(idx.row());
    QList<int> rows(seen.begin(), seen.end());
    std::sort(rows.begin(), rows.end());
    return rows;
}

void ListView::setSelectedIndex(int index) {
    const QAbstractItemModel* m = model();
    if (!m || index < 0 || index >= m->rowCount()) {
        clearSelection();
        clearSelectedIndicatorState();
        return;
    }
    const QModelIndex idx = m->index(index, 0);
    if (isVisible()) {
        setCurrentIndex(idx);
    } else {
        // Skip scrollTo before showing; the viewport is not laid out yet and
        // would scroll wrongly.
        // zh_CN: 未显示前不触发 scrollTo，避免 viewport 未布局时产生错误滚动。
        selectionModel()->setCurrentIndex(idx,
            QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Current | QItemSelectionModel::Rows);
    }
}

void ListView::applyPointerSelection(const QModelIndex& index, QMouseEvent* event)
{
    if (!selectionModel() || !index.isValid())
        return;

    switch (m_selectionMode) {
    case ListSelectionMode::None:
        selectionModel()->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
        return;
    case ListSelectionMode::Single:
        selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect |
                                                     QItemSelectionModel::Current |
                                                     QItemSelectionModel::Rows);
        return;
    case ListSelectionMode::Multiple:
        selectionModel()->select(index, QItemSelectionModel::Toggle |
                                        QItemSelectionModel::Rows);
        selectionModel()->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
        return;
    case ListSelectionMode::Extended:
        break;
    }

    const Qt::KeyboardModifiers modifiers = event ? event->modifiers() : Qt::NoModifier;
    const bool toggle = modifiers.testFlag(Qt::ControlModifier) ||
                        modifiers.testFlag(Qt::MetaModifier);
    const bool range = modifiers.testFlag(Qt::ShiftModifier);

    if (range && model()) {
        const QModelIndex anchor = currentIndex().isValid() ? currentIndex() : index;
        const int firstRow = qMin(anchor.row(), index.row());
        const int lastRow = qMax(anchor.row(), index.row());
        QItemSelection selection(model()->index(firstRow, index.column(), index.parent()),
                                 model()->index(lastRow, index.column(), index.parent()));
        selectionModel()->select(selection,
                                 (toggle ? QItemSelectionModel::Select
                                         : QItemSelectionModel::ClearAndSelect) |
                                     QItemSelectionModel::Rows);
        selectionModel()->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
        return;
    }

    if (toggle) {
        selectionModel()->select(index, QItemSelectionModel::Toggle |
                                        QItemSelectionModel::Rows);
        selectionModel()->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
        return;
    }

    selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect |
                                             QItemSelectionModel::Current |
                                             QItemSelectionModel::Rows);
}

void ListView::setSelectedIndicatorAnimationEnabled(bool enabled) {
    if (m_selectedIndicatorAnimationEnabled == enabled)
        return;
    m_selectedIndicatorAnimationEnabled = enabled;
    if (!enabled) {
        if (m_selectedIndicatorAnimation)
            m_selectedIndicatorAnimation->stop();
        clearMultiSelectedIndicatorState();
        setSelectedIndicatorProgress(1.0);
        if (viewport())
            viewport()->update();
    }
    emit selectedIndicatorAnimationEnabledChanged();
}

QRectF ListView::selectedIndicatorRect() const {
    return selectedIndicatorRect(m_selectedIndicatorProgress);
}

QRectF ListView::selectedIndicatorRect(qreal progress) const {
    if (!usesMovingSelectedIndicator())
        return {};
    if (!m_currentIndicatorIndex.isValid())
        return {};

    const QRectF target = selectedIndicatorBaseRect(QModelIndex(m_currentIndicatorIndex));
    if (target.isEmpty())
        return {};

    if (!m_previousIndicatorIndex.isValid() ||
        m_selectedIndicatorMotionDirection == IndicatorMotionDirection::None) {
        return target;
    }

    const QRectF previous = selectedIndicatorBaseRect(QModelIndex(m_previousIndicatorIndex));
    if (previous.isEmpty())
        return target;

    const qreal clamped = qBound(0.0, progress, 1.0);
    if (qFuzzyCompare(clamped + 1.0, 2.0))
        return target;
    if (qFuzzyCompare(clamped + 1.0, 1.0))
        return previous;
    return interpolatedSelectedIndicatorRect(previous, target, clamped);
}

QRectF ListView::selectedIndicatorRectForRow(int row) const {
    if (usesMovingSelectedIndicator())
        return row == selectedIndex() ? selectedIndicatorRect() : QRectF();

    if (!model() || row < 0 || row >= model()->rowCount())
        return {};
    const QModelIndex index = model()->index(row, 0);
    return selectedIndicatorRectForRow(row, multiSelectedIndicatorProgress(index));
}

QRectF ListView::selectedIndicatorRectForRow(int row, qreal progress) const {
    if (!model() || row < 0 || row >= model()->rowCount())
        return {};

    const QModelIndex index = model()->index(row, 0);
    if (!selectionModel() || !selectionModel()->isSelected(index))
        return {};

    const QRectF baseRect = selectedIndicatorBaseRect(index);
    if (baseRect.isEmpty())
        return {};

    if (usesMovingSelectedIndicator())
        return index == QModelIndex(m_currentIndicatorIndex) ? selectedIndicatorRect(progress) : QRectF();
    if (!usesRevealSelectedIndicators())
        return {};
    return revealedSelectedIndicatorRect(baseRect, progress);
}

::fluent::scrolling::ScrollBar* ListView::verticalFluentScrollBar() const {
    return m_vScrollBar;
}

::fluent::scrolling::ScrollBar* ListView::horizontalFluentScrollBar() const {
    return m_hScrollBar;
}

// ── Drag reorder events ───────────────────────────────────────────────────────

QPixmap ListView::renderItemPixmap(int row) const {
    if (!model() || row < 0 || row >= model()->rowCount())
        return {};

    QModelIndex idx = model()->index(row, 0);
    QRect rect = QListView::visualRect(idx);
    if (rect.isEmpty()) return {};

    // Use full viewport width for the snapshot so it looks like the real row.
    // If a SectionProxyDelegate is active, use the inner delegate directly
    // so the snapshot doesn't include section header area.
    QAbstractItemDelegate* del = itemDelegate();
    int h = rect.height();
    if (auto* proxy = dynamic_cast<SectionProxyDelegate*>(del)) {
        del = proxy->innerDelegate() ? proxy->innerDelegate() : del;
        // Subtract section header height for section-start rows
        if (proxy->isSectionStart(row))
            h -= proxy->sectionHeaderHeight();
    }
    const int w = viewport()->width();

    const qreal dpr = devicePixelRatioF();
    QPixmap pix(QSize(w, h) * dpr);
    pix.setDevicePixelRatio(dpr);
    // Fill with the real surface the list sits on, not a hardcoded bgLayer: the app installs no global
    // dark QPalette (it themes via paint), and the list is frequently hosted on a different surface
    // (e.g. the gallery sample card's bgLayerAlt), so a fixed bgLayer fill renders as a mismatched box
    // behind the dragged row. Walk up to the nearest ancestor advertising its painted background via
    // the "fluentSurfaceColor" property and fall back to bgLayer — the same approach FlipView uses for
    // its corner mask. zh_CN: 用列表真正所在的表面色填充，而非写死 bgLayer：本应用不装全局暗色 QPalette（靠绘制上主题），
    // 且列表常被托管在别的表面上（如画廊示例卡的 bgLayerAlt），写死 bgLayer 会在拖拽行后面显示成不匹配的色块。
    // 向上查找通过 "fluentSurfaceColor" 公布背景的祖先并回退到 bgLayer——与 FlipView 圆角遮罩的做法一致。
    QColor containerBg = themeColors().bgLayer;
    for (const QWidget* anc = parentWidget(); anc; anc = anc->parentWidget()) {
        const QVariant surfaceColor = anc->property("fluentSurfaceColor");
        if (!surfaceColor.isValid())
            continue;
        const QColor sc = surfaceColor.value<QColor>();
        if (sc.isValid() && sc.alpha() > 0)
            containerBg = sc;
        break;
    }
    pix.fill(containerBg);

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    QStyleOptionViewItem opt;
    FLUENT_INIT_VIEW_ITEM_OPTION(&opt);
    opt.rect = QRect(0, 0, w, h);
    opt.state |= QStyle::State_Selected | QStyle::State_Enabled;
    opt.state &= ~QStyle::State_MouseOver;
    del->paint(&p, opt, idx);
    p.end();

    return pix;
}

int ListView::dropIndicatorRow(const QPoint& pos) const {
    if (!model()) return 0;
    const int count = model()->rowCount();
    if (count == 0) return 0;

    // During drag, use displaced visual positions for hit testing
    // so the indicator follows the actual visual layout.
    if (m_isDragging) {
        for (int i = 0; i < count; ++i) {
            if (i == m_dragSourceRow) continue;
            QRect rect = QListView::visualRect(model()->index(i, 0));
            rect.translate(0, qRound(m_dragOffsets.value(i, 0.0)));
            if (pos.y() < rect.center().y())
                return i;
        }
        return count;
    }

    // Non-drag: standard hit test
    QModelIndex idx = indexAt(pos);
    if (!idx.isValid()) return count;
    QRect rect = visualRect(idx);
    if (pos.y() > rect.center().y()) return idx.row() + 1;
    return idx.row();
}

// ── Paint ─────────────────────────────────────────────────────────────────────

void ListView::paintEvent(QPaintEvent* event) {
    const auto& c = themeColors();
    const int r = CornerRadius::Control;

    // --- 1. Container background. zh_CN: 绘制容器背景。---
    if (m_backgroundVisible) {
        QPainter p(viewport());
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(viewport()->rect(), c.bgLayer);
        p.end();
    }

    // --- 2. Empty placeholder. zh_CN: 空列表占位符。---
    const bool isEmpty = !model() || model()->rowCount() == 0;
    if (isEmpty && !m_placeholderText.isEmpty()) {
        QPainter ph(viewport());
        ph.setRenderHint(QPainter::Antialiasing);
        ph.setPen(c.textTertiary);
        ph.setFont(themeFont(m_fontRole).toQFont());
        ph.drawText(viewport()->rect(), Qt::AlignCenter, m_placeholderText);
        ph.end();
    }

    // --- 3. Items via QListView's default painting. While dragging, the
    // visualRect override temporarily returns displaced rects. ---
    // zh_CN: 绘制列表项（QListView 默认绘制）；拖拽时通过 visualRect override
    // 临时返回偏移后的矩形。
    m_paintingWithOffsets = !m_dragOffsets.isEmpty();
    QListView::paintEvent(event);
    m_paintingWithOffsets = false;

    // --- 3.5 Section headers are now handled by SectionProxyDelegate ---

    if (m_pressedRow >= 0 && m_pressedHoverRow >= 0) {
        QPainter hp(viewport());
        hp.setRenderHint(QPainter::Antialiasing);
        paintPressedHoverFeedback(hp);
        hp.end();
    }

    // --- 3.6 Selection indicator overlay. zh_CN: 选中指示器覆盖层。---
    if (!m_isDragging) {
        QPainter ip(viewport());
        ip.setRenderHint(QPainter::Antialiasing);
        paintSelectedIndicator(ip);
        ip.end();
    }

    // --- 3.7 Drop indicator line. zh_CN: 绘制拖拽指示线。---
    if (m_isDragging && m_dropTargetRow >= 0 && model()) {
        m_paintingWithOffsets = !m_dragOffsets.isEmpty();
        QPainter dp(viewport());
        dp.setRenderHint(QPainter::Antialiasing);

        int y;
        if (m_dropTargetRow < model()->rowCount()) {
            y = visualRect(model()->index(m_dropTargetRow, 0)).top();
        } else {
            QRect lastRect = visualRect(model()->index(model()->rowCount() - 1, 0));
            y = lastRect.bottom() + 1;
        }

        const auto& clr = themeColors();
        dp.setPen(QPen(clr.accentDefault, 2.0));
        dp.drawLine(::Spacing::Padding::ListItemHorizontal, y,
                    viewport()->width() - ::Spacing::Padding::ListItemHorizontal, y);
        const int circleR = 3;
        dp.setBrush(clr.accentDefault);
        dp.setPen(Qt::NoPen);
        dp.drawEllipse(QPoint(::Spacing::Padding::ListItemHorizontal, y), circleR, circleR);
        dp.drawEllipse(QPoint(viewport()->width() - ::Spacing::Padding::ListItemHorizontal, y), circleR, circleR);
        m_paintingWithOffsets = false;
        dp.end();
    }

    // --- 3.8 Drag ghost layer. zh_CN: 拖拽浮动图层。---
    if (m_isDragging && !m_dragPixmap.isNull()) {
        QPainter fp(viewport());
        fp.setRenderHint(QPainter::Antialiasing);
        fp.setOpacity(0.85);
        const int pixH = qRound(m_dragPixmap.height() / m_dragPixmap.devicePixelRatio());
        QPoint pixPos(0, m_dragCurrentPos.y() - pixH / 2);
        fp.drawPixmap(pixPos, m_dragPixmap);
        fp.end();
    }

    // --- 4. Corner mask: cover the four corners with the parent background
    // (antialiased, replaces setMask). zh_CN: 圆角遮罩——用父背景色覆盖四角。---
    if (m_borderVisible || m_backgroundVisible) {
        QPainter cp(viewport());
        cp.setRenderHint(QPainter::Antialiasing);

        QPainterPath fullRect;
        fullRect.addRect(QRectF(viewport()->rect()));
        QPainterPath roundedArea;
        roundedArea.addRoundedRect(QRectF(viewport()->rect()), r, r);
        QPainterPath corners = fullRect - roundedArea;

        // Take the parent's background, falling back to bgCanvas. zh_CN: 从父控件取背景色，回退到 bgCanvas。
        QColor parentBg = c.bgCanvas;
        if (parentWidget()) {
            const QPalette& pp = parentWidget()->palette();
            if (pp.color(QPalette::Window).alpha() > 0)
                parentBg = pp.color(QPalette::Window);
        }
        cp.fillPath(corners, parentBg);
        cp.end();
    }

    // --- 5. Container border. zh_CN: 容器边框。---
    if (m_borderVisible) {
        QPainter bp(viewport());
        bp.setRenderHint(QPainter::Antialiasing);

        QPainterPath borderPath;
        borderPath.addRoundedRect(QRectF(viewport()->rect()).adjusted(0.5, 0.5, -0.5, -0.5), r, r);
        bp.setPen(QPen(c.strokeDefault, 1.0));
        bp.setBrush(Qt::NoBrush);
        bp.drawPath(borderPath);
        bp.end();
    }
}

// ── Layout ────────────────────────────────────────────────────────────────────

void ListView::resizeEvent(QResizeEvent* event) {
    QListView::resizeEvent(event);
    syncFluentScrollBar();
    syncFluentHScrollBar();
    layoutHeader();
    layoutFooter();
    if (usesMovingSelectedIndicator())
        refreshSelectedIndicatorGeometry(false);
    else if (viewport())
        viewport()->update();
}

void ListView::showEvent(QShowEvent* event) {
    QListView::showEvent(event);
    updateViewportMargins();
    syncFluentScrollBar();
    syncFluentHScrollBar();
    layoutHeader();
    layoutFooter();
    // On first show, re-anchor to the current selection with the proper
    // viewport size.
    // zh_CN: 初次显示时用正确的 viewport 尺寸重新定位到当前选中项。
    if (currentIndex().isValid()) {
        scrollTo(currentIndex(), QAbstractItemView::EnsureVisible);
    }
    if (usesMovingSelectedIndicator()) {
        updateSelectedIndicatorFromSelection();
        refreshSelectedIndicatorGeometry(true);
    } else if (usesRevealSelectedIndicators()) {
        syncMultiSelectedIndicators(QItemSelection(), QItemSelection());
    }
    QTimer::singleShot(0, this, [this]() {
        syncFluentScrollBar();
        syncFluentHScrollBar();
        if (usesMovingSelectedIndicator())
            refreshSelectedIndicatorGeometry(true);
        else if (viewport())
            viewport()->update();
    });
}

bool ListView::isPointInSectionHeader(const QPoint& viewportPos) const {
    if (!m_sectionEnabled || !m_sectionProxy) return false;

    auto* proxy = static_cast<SectionProxyDelegate*>(m_sectionProxy);
    if (!proxy) return false;

    QModelIndex idx = indexAt(viewportPos);
    if (!idx.isValid()) return false;

    if (!proxy->isSectionStart(idx.row())) return false;

    // The section header occupies the top portion of the visual rect
    QRect vr = visualRect(idx);
    int headerH = proxy->sectionHeaderHeight();
    return viewportPos.y() < vr.top() + headerH;
}

void ListView::mousePressEvent(QMouseEvent* event) {
    m_pressedRow = -1;
    updatePressedHoverRow(-1);
    m_dragSourceRow = -1;

    if (isPointInSectionHeader(event->pos())) {
        event->accept();
        return;  // Swallow click on section header area
    }

    if (event->button() == Qt::LeftButton) {
        const QModelIndex idx = indexAt(event->pos());
        if (idx.isValid()) {
            m_pressedRow = idx.row();
            updatePressedHoverRow(idx.row());
            m_dragStartPos = event->pos();
            if (m_canReorderItems)
                m_dragSourceRow = idx.row();
            setFocus(Qt::MouseFocusReason);
            event->accept();
            return;
        }
    }

    QListView::mousePressEvent(event);
}

void ListView::mouseMoveEvent(QMouseEvent* event) {
    if (m_canReorderItems && m_dragSourceRow >= 0 && (event->buttons() & Qt::LeftButton)) {
        if (!m_isDragging) {
            if ((event->pos() - m_dragStartPos).manhattanLength() >= QApplication::startDragDistance()) {
                // Grab snapshot BEFORE setting m_isDragging to avoid capturing ghost overlay
                m_dragPixmap = renderItemPixmap(m_dragSourceRow);
                m_isDragging = true;
                updatePressedHoverRow(-1);
                if (usesMovingSelectedIndicator())
                    refreshSelectedIndicatorGeometry(true);
            }
        }

        if (m_isDragging) {
            m_dragCurrentPos = event->pos();
            int target = dropIndicatorRow(event->pos());
            if (target != m_dropTargetRow) {
                m_dropTargetRow = target;
                updateDragDisplacement();
            }
            viewport()->update();
            event->accept();
            return;
        }
    }

    if (m_pressedRow >= 0 && (event->buttons() & Qt::LeftButton)) {
        if (isPointInSectionHeader(event->pos())) {
            updatePressedHoverRow(-1);
        } else {
            const QModelIndex hover = indexAt(event->pos());
            updatePressedHoverRow(hover.isValid() ? hover.row() : -1);
        }
        event->accept();
        return;
    }

    QListView::mouseMoveEvent(event);
}

void ListView::mouseReleaseEvent(QMouseEvent* event) {
    if (m_isDragging && event->button() == Qt::LeftButton) {
        int src = m_dragSourceRow;
        int dst = m_dropTargetRow;

        if (dst >= 0 && src >= 0 && model()) {
            if (src < dst) dst--;
            if (src != dst && src < model()->rowCount()) {
                int destRow = (src < dst) ? dst + 1 : dst;
                bool moved = model()->moveRow(QModelIndex(), src, QModelIndex(), destRow);
                if (!moved) {
                    // QStandardItemModel doesn't implement moveRow — use takeRow/insertRow
                    if (auto* sim = qobject_cast<QStandardItemModel*>(model())) {
                        auto row = sim->takeRow(src);
                        sim->insertRow(dst, row);
                        moved = true;
                    }
                }
                if (moved) {
                    setCurrentIndex(model()->index(dst, 0));
                    emit itemReordered(src, dst);
                }
            }
        }

        m_isDragging = false;
        m_pressedRow = -1;
        updatePressedHoverRow(-1);
        m_dragSourceRow = -1;
        m_dropTargetRow = -1;
        m_dragPixmap = QPixmap();
        clearDragAnimations();
        if (usesMovingSelectedIndicator())
            refreshSelectedIndicatorGeometry(true);
        else if (viewport())
            viewport()->update();
        viewport()->update();
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton && m_pressedRow >= 0) {
        const QModelIndex released = indexAt(event->pos());
        const bool clickOnPressedItem =
            released.isValid() && released.row() == m_pressedRow;
        if (clickOnPressedItem) {
            applyPointerSelection(released, event);
            emit itemClicked(released.row());
        }
        m_pressedRow = -1;
        updatePressedHoverRow(-1);
        m_dragSourceRow = -1;
        event->accept();
        return;
    }

    if (m_canReorderItems) {
        m_pressedRow = -1;
        updatePressedHoverRow(-1);
        m_dragSourceRow = -1;
    }
    QListView::mouseReleaseEvent(event);
}

void ListView::enterEvent(FluentEnterEvent* event) {
    setViewportHovered(true);
    QListView::enterEvent(event);
}

void ListView::leaveEvent(QEvent* event) {
    setViewportHovered(false);
    if (m_pressedRow >= 0)
        updatePressedHoverRow(-1);
    QListView::leaveEvent(event);
}

// ── Overscroll bounce ─────────────────────────────────────────────────────────
//
// Cross-platform wheel input handling — see openspec listview-cross-platform-input/design.md.
// Events are classified into three paths:
//
//   PhaseBased       phase != NoScrollPhase                   macOS native trackpad
//   NoPhasePixel     phase == NoScrollPhase, pixelDelta != 0  some Win precision touchpad drivers
//   NoPhaseDiscrete  phase == NoScrollPhase, pixelDelta == 0  mouse wheel / Mac RDP→Windows / Qt5
//
// PhaseBased / NoPhasePixel keep the original behavior (smooth pixel scrolling + boundary
// overscroll). NoPhaseDiscrete uses pixel scrolling first, then starts a bounded one-shot
// boundary bounce while consuming same-direction tails to prevent RDP flap.

void ListView::wheelEvent(QWheelEvent* event) {
    m_overscroll->handleWheel(event);
}

void ListView::currentChanged(const QModelIndex& current, const QModelIndex& previous) {
    QListView::currentChanged(current, previous);
    Q_UNUSED(current)
    if (usesMovingSelectedIndicator())
        updateSelectedIndicatorFromSelection(previous);
}

void ListView::selectionChanged(const QItemSelection& selected, const QItemSelection& deselected) {
    QListView::selectionChanged(selected, deselected);
    if (usesMovingSelectedIndicator())
        updateSelectedIndicatorFromSelection();
    else
        syncMultiSelectedIndicators(selected, deselected);
}

void ListView::scrollContentsBy(int dx, int dy) {
    QListView::scrollContentsBy(dx, dy);
    if (usesMovingSelectedIndicator())
        refreshSelectedIndicatorGeometry(false);
    else if (viewport())
        viewport()->update();
}

int ListView::verticalOffset() const {
    // The controller tracks one overscroll value for the active scroll axis; apply it to the
    // matching offset. m_overscroll may be null during base construction. zh_CN: 控制器只跟踪当前
    // 滚动轴的一个 overscroll 值，按轴应用到对应偏移；构造期间 m_overscroll 可能尚未创建。
    const qreal overscroll = (m_overscroll && flow() != LeftToRight) ? m_overscroll->value() : 0.0;
    return QListView::verticalOffset() - qRound(overscroll);
}

int ListView::horizontalOffset() const {
    const qreal overscroll = (m_overscroll && flow() == LeftToRight) ? m_overscroll->value() : 0.0;
    return QListView::horizontalOffset() - qRound(overscroll);
}

QRect ListView::visualRect(const QModelIndex& index) const {
    QRect r = QListView::visualRect(index);
    if (m_paintingWithOffsets && index.isValid()) {
        if (m_isDragging && index.row() == m_dragSourceRow) {
            // Hide source row: move off-screen so QListView won't paint it
            r.moveTop(-r.height() * 2);
            return r;
        }
        r.translate(0, qRound(m_dragOffsets.value(index.row(), 0.0)));
    }
    return r;
}

void ListView::connectSelectedIndicatorModel(QAbstractItemModel* model) {
    if (!model)
        return;

    m_indicatorModelAboutToResetConnection = connect(model, &QAbstractItemModel::modelAboutToBeReset,
                                                     this, &ListView::clearSelectedIndicatorState);
    m_indicatorModelResetConnection = connect(model, &QAbstractItemModel::modelReset,
                                             this, &ListView::clearSelectedIndicatorState);
    m_indicatorRowsAboutToBeRemovedConnection = connect(model, &QAbstractItemModel::rowsAboutToBeRemoved,
                                                        this, [this](const QModelIndex&, int, int) {
                                                            clearSelectedIndicatorState();
                                                        });
    m_indicatorRowsMovedConnection = connect(model, &QAbstractItemModel::rowsMoved,
                                             this, [this](const QModelIndex&, int, int, const QModelIndex&, int) {
                                                 if (usesMovingSelectedIndicator())
                                                     refreshSelectedIndicatorGeometry(true);
                                                 else if (viewport())
                                                     viewport()->update();
                                             });
    m_indicatorLayoutChangedConnection = connect(model, &QAbstractItemModel::layoutChanged,
                                                this, [this]() {
                                                    if (usesMovingSelectedIndicator())
                                                        refreshSelectedIndicatorGeometry(true);
                                                    else if (viewport())
                                                        viewport()->update();
                                                });
}

void ListView::disconnectSelectedIndicatorModel() {
    QObject::disconnect(m_indicatorModelAboutToResetConnection);
    QObject::disconnect(m_indicatorModelResetConnection);
    QObject::disconnect(m_indicatorRowsAboutToBeRemovedConnection);
    QObject::disconnect(m_indicatorRowsMovedConnection);
    QObject::disconnect(m_indicatorLayoutChangedConnection);
    m_indicatorModelAboutToResetConnection = QMetaObject::Connection();
    m_indicatorModelResetConnection = QMetaObject::Connection();
    m_indicatorRowsAboutToBeRemovedConnection = QMetaObject::Connection();
    m_indicatorRowsMovedConnection = QMetaObject::Connection();
    m_indicatorLayoutChangedConnection = QMetaObject::Connection();
}

void ListView::clearSelectedIndicatorState() {
    if (m_selectedIndicatorAnimation)
        m_selectedIndicatorAnimation->stop();
    clearMultiSelectedIndicatorState();
    m_previousIndicatorIndex = QPersistentModelIndex();
    m_currentIndicatorIndex = QPersistentModelIndex();
    setSelectedIndicatorMotionDirection(IndicatorMotionDirection::None);
    setSelectedIndicatorProgress(1.0);
    if (viewport())
        viewport()->update();
}

void ListView::clearMultiSelectedIndicatorState() {
    for (auto it = m_multiIndicatorAnimations.begin(); it != m_multiIndicatorAnimations.end(); ++it) {
        if (it.value()) {
            it.value()->stop();
            it.value()->deleteLater();
        }
    }
    m_multiIndicatorAnimations.clear();
}

void ListView::updateSelectedIndicatorFromSelection(const QModelIndex& previous) {
    updateSelectedIndicatorForIndex(normalizedSelectedModelIndex(), previous);
}

void ListView::updateSelectedIndicatorForIndex(const QModelIndex& current, const QModelIndex& previous) {
    if (!current.isValid() || !isSelectedIndicatorEndpointUsable(current)) {
        clearSelectedIndicatorState();
        return;
    }

    if (m_currentIndicatorIndex.isValid() && current == m_currentIndicatorIndex) {
        refreshSelectedIndicatorGeometry(false);
        return;
    }

    QPersistentModelIndex previousIndex = m_currentIndicatorIndex;
    if (!previousIndex.isValid() && previous.isValid())
        previousIndex = QPersistentModelIndex(previous);

    const QRectF targetRect = selectedIndicatorBaseRect(current);
    if (targetRect.isEmpty()) {
        clearSelectedIndicatorState();
        return;
    }

    const bool hasUsablePrevious = previousIndex.isValid() && isSelectedIndicatorEndpointUsable(previousIndex);

    const auto direction = hasUsablePrevious
                               ? classifySelectedIndicatorDirection(previousIndex, current)
                               : IndicatorMotionDirection::None;
    const bool canAnimate = hasUsablePrevious && direction != IndicatorMotionDirection::None && !m_isDragging;

    m_previousIndicatorIndex = canAnimate ? previousIndex : QPersistentModelIndex();
    m_currentIndicatorIndex = QPersistentModelIndex(current);
    setSelectedIndicatorMotionDirection(canAnimate ? direction : IndicatorMotionDirection::None);
    startSelectedIndicatorAnimation(canAnimate && m_selectedIndicatorAnimationEnabled);
}

QModelIndex ListView::normalizedSelectedModelIndex() const {
    const QAbstractItemModel* m = model();
    const int row = selectedIndex();
    if (!m || row < 0 || row >= m->rowCount())
        return {};
    return m->index(row, 0);
}

bool ListView::isSelectedIndicatorEndpointUsable(const QModelIndex& index) const {
    if (!index.isValid() || index.model() != model())
        return false;
    if (isRowHidden(index.row()))
        return false;
    return !selectedIndicatorBaseRect(index).isEmpty();
}

bool ListView::usesMovingSelectedIndicator() const {
    return m_selectionMode == ListSelectionMode::Single;
}

bool ListView::usesRevealSelectedIndicators() const {
    return m_selectionMode == ListSelectionMode::Multiple ||
           m_selectionMode == ListSelectionMode::Extended;
}

QRectF ListView::selectedIndicatorBaseRect(const QModelIndex& index) const {
    if (!index.isValid() || index.model() != model() || !viewport())
        return {};

    const QRect itemRect = QListView::visualRect(index);
    if (itemRect.isEmpty() || !viewport()->rect().intersects(itemRect))
        return {};

    const QRectF bgRect = QRectF(itemRect).adjusted(2.0, 1.0, -2.0, -1.0);
    if (bgRect.isEmpty())
        return {};

    if (flow() == LeftToRight) {
        const qreal indicatorH = 3.0;
        const qreal indicatorW = qBound(16.0, bgRect.width() * 0.45, 24.0);
        const qreal indicatorX = bgRect.center().x() - indicatorW / 2.0;
        const qreal indicatorY = bgRect.bottom() - indicatorH - 4.0;
        return QRectF(indicatorX, indicatorY, indicatorW, indicatorH);
    }

    const qreal indicatorW = 3.0;
    const qreal indicatorH = 16.0;
    const qreal indicatorX = bgRect.left() + 4.0;
    const qreal indicatorY = bgRect.center().y() - indicatorH / 2.0;
    return QRectF(indicatorX, indicatorY, indicatorW, indicatorH);
}

QRectF ListView::revealedSelectedIndicatorRect(const QRectF& baseRect, qreal progress) const {
    if (baseRect.isEmpty())
        return {};

    const qreal clamped = qBound(0.0, progress, 1.0);
    const qreal scale = 0.35 + 0.65 * clamped;
    if (flow() == LeftToRight) {
        const qreal width = baseRect.width() * scale;
        return QRectF(baseRect.center().x() - width / 2.0,
                      baseRect.top(),
                      width,
                      baseRect.height());
    }

    const qreal height = baseRect.height() * scale;
    return QRectF(baseRect.left(),
                  baseRect.center().y() - height / 2.0,
                  baseRect.width(),
                  height);
}

QRectF ListView::interpolatedSelectedIndicatorRect(const QRectF& previous, const QRectF& target, qreal progress) const {
    const qreal leading = indicatorLeadingProgress(progress);
    const qreal trailing = indicatorTrailingProgress(progress);

    qreal left = lerp(previous.left(), target.left(), progress);
    qreal right = lerp(previous.right(), target.right(), progress);
    qreal top = lerp(previous.top(), target.top(), progress);
    qreal bottom = lerp(previous.bottom(), target.bottom(), progress);

    switch (m_selectedIndicatorMotionDirection) {
    case IndicatorMotionDirection::Down:
        top = lerp(previous.top(), target.top(), trailing);
        bottom = lerp(previous.bottom(), target.bottom(), leading);
        left = target.left();
        right = target.right();
        break;
    case IndicatorMotionDirection::Up:
        top = lerp(previous.top(), target.top(), leading);
        bottom = lerp(previous.bottom(), target.bottom(), trailing);
        left = target.left();
        right = target.right();
        break;
    case IndicatorMotionDirection::Right:
        left = lerp(previous.left(), target.left(), trailing);
        right = lerp(previous.right(), target.right(), leading);
        top = target.top();
        bottom = target.bottom();
        break;
    case IndicatorMotionDirection::Left:
        left = lerp(previous.left(), target.left(), leading);
        right = lerp(previous.right(), target.right(), trailing);
        top = target.top();
        bottom = target.bottom();
        break;
    case IndicatorMotionDirection::None:
        return target;
    }

    const qreal normalizedLeft = qMin(left, right);
    const qreal normalizedTop = qMin(top, bottom);
    const qreal normalizedRight = qMax(left, right);
    const qreal normalizedBottom = qMax(top, bottom);
    return QRectF(QPointF(normalizedLeft, normalizedTop), QPointF(normalizedRight, normalizedBottom));
}

ListView::IndicatorMotionDirection ListView::classifySelectedIndicatorDirection(const QModelIndex& previous, const QModelIndex& current) const {
    if (!previous.isValid() || !current.isValid() || previous == current)
        return IndicatorMotionDirection::None;

    const QRectF previousRect = selectedIndicatorBaseRect(previous);
    const QRectF currentRect = selectedIndicatorBaseRect(current);
    if (previousRect.isEmpty() || currentRect.isEmpty())
        return IndicatorMotionDirection::None;

    if (flow() == LeftToRight) {
        if (!qFuzzyCompare(previousRect.center().x() + 1.0, currentRect.center().x() + 1.0)) {
            return currentRect.center().x() > previousRect.center().x()
                       ? IndicatorMotionDirection::Right
                       : IndicatorMotionDirection::Left;
        }
        if (current.row() != previous.row()) {
            return current.row() > previous.row()
                       ? IndicatorMotionDirection::Right
                       : IndicatorMotionDirection::Left;
        }
        return IndicatorMotionDirection::None;
    }

    if (!qFuzzyCompare(previousRect.center().y() + 1.0, currentRect.center().y() + 1.0)) {
        return currentRect.center().y() > previousRect.center().y()
                   ? IndicatorMotionDirection::Down
                   : IndicatorMotionDirection::Up;
    }
    if (current.row() != previous.row()) {
        return current.row() > previous.row()
                   ? IndicatorMotionDirection::Down
                   : IndicatorMotionDirection::Up;
    }
    return IndicatorMotionDirection::None;
}

void ListView::startSelectedIndicatorAnimation(bool animated) {
    if (!m_selectedIndicatorAnimation)
        return;

    m_selectedIndicatorAnimation->stop();
    if (!animated) {
        setSelectedIndicatorProgress(1.0);
        if (viewport())
            viewport()->update();
        return;
    }

    setSelectedIndicatorProgress(0.0);
    m_selectedIndicatorAnimation->setStartValue(0.0);
    m_selectedIndicatorAnimation->setEndValue(1.0);
    m_selectedIndicatorAnimation->start();
}

void ListView::syncMultiSelectedIndicators(const QItemSelection& selected, const QItemSelection& deselected) {
    if (!usesRevealSelectedIndicators() || !selectionModel()) {
        clearMultiSelectedIndicatorState();
        return;
    }

    for (auto it = m_multiIndicatorAnimations.begin(); it != m_multiIndicatorAnimations.end();) {
        const QModelIndex index = it.key();
        if (!index.isValid() || index.model() != model() || !selectionModel()->isSelected(index)) {
            if (it.value()) {
                it.value()->stop();
                it.value()->deleteLater();
            }
            it = m_multiIndicatorAnimations.erase(it);
        } else {
            ++it;
        }
    }

    for (const QModelIndex& index : deselected.indexes()) {
        const QPersistentModelIndex persistent(index);
        if (auto* anim = m_multiIndicatorAnimations.take(persistent)) {
            anim->stop();
            anim->deleteLater();
        }
    }

    for (const QModelIndex& index : selected.indexes()) {
        if (index.isValid() && index.column() == 0 && selectionModel()->isSelected(index))
            startMultiSelectedIndicatorReveal(index);
    }

    if (selectionModel()->selectedIndexes().isEmpty())
        clearMultiSelectedIndicatorState();

    if (viewport())
        viewport()->update();
}

void ListView::startMultiSelectedIndicatorReveal(const QModelIndex& index) {
    if (!index.isValid() || index.column() != 0)
        return;

    const QPersistentModelIndex persistent(index);
    if (!m_selectedIndicatorAnimationEnabled || m_multiIndicatorAnimations.contains(persistent))
        return;

    auto* animation = new QVariantAnimation(this);
    animation->setDuration(themeAnimation().fast);
    animation->setEasingCurve(themeAnimation().decelerate);
    animation->setStartValue(0.0);
    animation->setEndValue(1.0);
    m_multiIndicatorAnimations.insert(persistent, animation);
    connect(animation, &QVariantAnimation::valueChanged, this, [this](const QVariant&) {
        if (viewport())
            viewport()->update();
    });
    connect(animation, &QVariantAnimation::finished, this, [this, persistent, animation]() {
        m_multiIndicatorAnimations.remove(persistent);
        animation->deleteLater();
        if (viewport())
            viewport()->update();
    });
    animation->start();
}

qreal ListView::multiSelectedIndicatorProgress(const QModelIndex& index) const {
    const auto it = m_multiIndicatorAnimations.find(QPersistentModelIndex(index));
    return it == m_multiIndicatorAnimations.end() ? 1.0 : it.value()->currentValue().toReal();
}

void ListView::setSelectedIndicatorProgress(qreal progress) {
    const qreal clamped = qBound(0.0, progress, 1.0);
    if (qFuzzyCompare(m_selectedIndicatorProgress + 1.0, clamped + 1.0))
        return;
    m_selectedIndicatorProgress = clamped;
    emit selectedIndicatorProgressChanged();
}

void ListView::setSelectedIndicatorMotionDirection(IndicatorMotionDirection direction) {
    if (m_selectedIndicatorMotionDirection == direction)
        return;
    m_selectedIndicatorMotionDirection = direction;
    emit selectedIndicatorMotionDirectionChanged();
}

void ListView::refreshSelectedIndicatorGeometry(bool snapToTarget) {
    if (!m_currentIndicatorIndex.isValid()) {
        const QModelIndex selected = normalizedSelectedModelIndex();
        if (selected.isValid())
            updateSelectedIndicatorForIndex(selected, QModelIndex());
        return;
    }

    const QModelIndex current(m_currentIndicatorIndex);
    const QRectF target = selectedIndicatorBaseRect(current);
    if (target.isEmpty()) {
        clearSelectedIndicatorState();
        return;
    }

    if (snapToTarget || !m_previousIndicatorIndex.isValid()) {
        m_previousIndicatorIndex = QPersistentModelIndex();
        setSelectedIndicatorMotionDirection(IndicatorMotionDirection::None);
        startSelectedIndicatorAnimation(false);
        return;
    }

    const QRectF previous = selectedIndicatorBaseRect(m_previousIndicatorIndex);
    if (previous.isEmpty()) {
        m_previousIndicatorIndex = QPersistentModelIndex();
        setSelectedIndicatorMotionDirection(IndicatorMotionDirection::None);
        startSelectedIndicatorAnimation(false);
        return;
    }

    if (viewport())
        viewport()->update();
}

void ListView::paintSelectedIndicator(QPainter& painter) const {
    // M3/macOS carry selection via the delegate's full-row fill, not a Fluent accent pill — suppress it.
    // zh_CN: M3/macOS 的选择由委托整行填充承载,而非 Fluent accent 指示条——在此抑制。
    if (themeDesignLanguage() != DesignFluent)
        return;

    if (!themeColors().accentDefault.isValid() || !selectionModel())
        return;

    if (usesMovingSelectedIndicator()) {
        paintIndicatorRect(painter, selectedIndicatorRect());
        return;
    }

    if (!usesRevealSelectedIndicators())
        return;

    QSet<int> paintedRows;
    for (const QModelIndex& index : selectionModel()->selectedIndexes()) {
        if (!index.isValid() || index.column() != 0 || paintedRows.contains(index.row()))
            continue;
        paintedRows.insert(index.row());
        const qreal progress = multiSelectedIndicatorProgress(index);
        paintIndicatorRect(painter, selectedIndicatorRectForRow(index.row(), progress), progress);
    }
}

void ListView::paintIndicatorRect(QPainter& painter, const QRectF& indicatorRect, qreal opacity) const {
    if (indicatorRect.isEmpty() || !themeColors().accentDefault.isValid())
        return;

    const bool horizontalIndicator = flow() == LeftToRight;
    const qreal radius = horizontalIndicator
                             ? indicatorRect.height() / 2.0
                             : indicatorRect.width() / 2.0;
    QPainterPath path;
    path.addRoundedRect(indicatorRect, radius, radius);
    QColor accent = themeColors().accentDefault;
    accent.setAlphaF(accent.alphaF() * qBound(0.0, opacity, 1.0));
    painter.setPen(Qt::NoPen);
    painter.setBrush(accent);
    painter.drawPath(path);
}

// paintSectionHeaders() removed — section headers are now painted by SectionProxyDelegate

void ListView::setViewportHovered(bool hovered) {
    if (m_viewportHovered == hovered)
        return;
    m_viewportHovered = hovered;
    emit viewportHoveredChanged();
}

void ListView::updatePressedHoverRow(int row) {
    if (m_pressedHoverRow == row)
        return;
    m_pressedHoverRow = row;
    if (viewport())
        viewport()->update();
}

void ListView::paintPressedHoverFeedback(QPainter& painter) const {
    if (!model() || m_pressedHoverRow < 0 || m_pressedHoverRow >= model()->rowCount())
        return;

    const QModelIndex index = model()->index(m_pressedHoverRow, 0);
    if (!index.isValid())
        return;

    const QRect rect = visualRect(index);
    if (rect.isEmpty() || !rect.intersects(viewport()->rect()))
        return;

    QStyleOptionViewItem option;
    FLUENT_INIT_VIEW_ITEM_OPTION(&option);
    option.rect = rect;
    option.widget = viewport();
    option.font = font();
    option.state |= QStyle::State_MouseOver;
    if (isEnabled()) {
        option.state |= QStyle::State_Enabled;
    } else {
        option.state &= ~QStyle::State_Enabled;
    }
    if (hasFocus())
        option.state |= QStyle::State_Active;
    if (selectionModel() && selectionModel()->isSelected(index))
        option.state |= QStyle::State_Selected;
    if (m_pressedHoverRow == m_pressedRow)
        option.state |= QStyle::State_Sunken;

    itemDelegate()->paint(&painter, option, index);
}

// ── Theme ─────────────────────────────────────────────────────────────────────

void ListView::onThemeUpdated() {
    applyThemeStyle();
}

void ListView::applyThemeStyle() {
    const auto& c = themeColors();

    QPalette pal = palette();
    pal.setColor(QPalette::Base, Qt::transparent);
    pal.setColor(QPalette::Window, Qt::transparent);
    pal.setColor(QPalette::Text, c.textPrimary);
    pal.setColor(QPalette::Highlight, Qt::transparent);
    pal.setColor(QPalette::HighlightedText, c.textPrimary);
    setPalette(pal);

    setFont(themeFont(m_fontRole).toQFont());

    if (viewport()) {
        viewport()->setAutoFillBackground(false);
        QPalette vpal = viewport()->palette();
        vpal.setColor(QPalette::Base, Qt::transparent);
        vpal.setColor(QPalette::Window, Qt::transparent);
        viewport()->setPalette(vpal);
    }

    // Header label theme (only for internally-created labels). Color via the label's own style sheet
    // so an ancestor style sheet (e.g. the gallery sample card) can't clobber it to near-black.
    if (m_ownsHeader) {
        if (auto* lbl = qobject_cast<QLabel*>(m_header)) {
            lbl->setFont(themeFont(Typography::FontRole::Subtitle).toQFont());
            lbl->setStyleSheet(QStringLiteral("color: %1; background: transparent;")
                                   .arg(cssRgba(c.textPrimary)));
        }
    }

    // Footer label theme (only for internally-created labels)
    if (m_ownsFooter) {
        if (auto* lbl = qobject_cast<QLabel*>(m_footer)) {
            lbl->setFont(themeFont(Typography::FontRole::Caption).toQFont());
            lbl->setStyleSheet(QStringLiteral("color: %1; background: transparent;")
                                   .arg(cssRgba(c.textSecondary)));
        }
    }

    updateViewportMargins();
    layoutHeader();
    layoutFooter();
    if (usesMovingSelectedIndicator())
        refreshSelectedIndicatorGeometry(false);
    update();
}

// ── Internal layout helpers ───────────────────────────────────────────────────

void ListView::layoutHeader() {
    if (!m_header || !m_header->isVisible()) return;

    const int headerH = m_header->sizeHint().height() + ::Spacing::Gap::Normal;
    m_header->setGeometry(0, 0, width(), headerH);
    m_header->raise();
}

void ListView::layoutFooter() {
    if (!m_footer || !m_footer->isVisible()) return;

    const int footerH = m_footer->sizeHint().height() + ::Spacing::Gap::Normal;
    m_footer->setGeometry(0, height() - footerH, width(), footerH);
    m_footer->raise();
}

void ListView::updateViewportMargins() {
    int top = 0, bottom = 0;
    if (m_header && m_header->isVisible()) {
        top = m_header->sizeHint().height() + ::Spacing::Gap::Normal;
    }
    if (m_footer && m_footer->isVisible()) {
        bottom = m_footer->sizeHint().height() + ::Spacing::Gap::Normal;
    }
    setViewportMargins(0, top, 0, bottom);
}

/**
 * @brief Re-syncs the fluent vertical bar: suppress, mirror, then place.
 * zh_CN: 重新同步 Fluent 纵向滚动条：压制原生条、镜像模型、再摆放。
 *
 * Platform styles may re-show the native bars after show/resize, so this
 * runs repeatedly. The shared steps live in OverlayScrollChrome; only the
 * header-aware anchor math stays here.
 * zh_CN: 平台/样式可能在 show/resize 后重新显示原生条，因此需反复调用。
 * 共享步骤收敛在 OverlayScrollChrome，这里只保留感知 header 的锚点计算。
 */
void ListView::syncFluentScrollBar() {
    ::fluent::scrolling::suppressNativeScrollBars(verticalScrollBar(), horizontalScrollBar());
    if (!m_vScrollBar) return;
    if (!::fluent::scrolling::mirrorNativeScrollBar(m_vScrollBar, verticalScrollBar()))
        return;

    const QRect r = rect();
    const int top = (m_header && m_header->isVisible())
                        ? m_header->geometry().bottom() + 2
                        : r.top() + 2;
    ::fluent::scrolling::placeVerticalScrollBar(m_vScrollBar, r, top,
                                                kScrollBarEdgeInset, /*bottomInset=*/2);
}

void ListView::syncFluentHScrollBar() {
    ::fluent::scrolling::suppressNativeScrollBars(verticalScrollBar(), horizontalScrollBar());
    if (!m_hScrollBar) return;
    if (!::fluent::scrolling::mirrorNativeScrollBar(m_hScrollBar, horizontalScrollBar()))
        return;

    ::fluent::scrolling::placeHorizontalScrollBar(m_hScrollBar, rect(),
                                                  /*leftInset=*/2, /*rightInset=*/2,
                                                  /*bottomInset=*/0);
}

void ListView::refreshFluentScrollChrome() {
    syncFluentScrollBar();
    syncFluentHScrollBar();
}

// ── Drag displacement animation ───────────────────────────────────────────────

void ListView::updateDragDisplacement() {
    if (m_dragSourceRow < 0 || m_dropTargetRow < 0 || !model()) {
        clearDragAnimations();
        return;
    }

    const int itemCount = model()->rowCount();
    const int src = m_dragSourceRow;
    const int dst = m_dropTargetRow;

    // Use the actual source row height for displacement amount
    const int srcH = QListView::visualRect(model()->index(src, 0)).height();
    if (srcH <= 0) return;

    for (int i = 0; i < itemCount; ++i) {
        qreal target = 0.0;
        if (i == src) {
            // Source item: no displacement (follows cursor via drag pixmap)
            target = 0.0;
        } else if (src < dst && i > src && i < dst) {
            // Source moves down: items between (src, dst) shift up
            target = -srcH;
        } else if (src > dst && i >= dst && i < src) {
            // Source moves up: items between [dst, src) shift down
            target = srcH;
        }

        const qreal current = m_dragOffsets.value(i, 0.0);
        if (qFuzzyCompare(current, target) && !m_dragAnims.contains(i)) {
            continue;
        }

        if (auto* oldAnim = m_dragAnims.value(i)) {
            oldAnim->stop();
            oldAnim->deleteLater();
            m_dragAnims.remove(i);
        }

        if (qFuzzyCompare(current, target)) {
            continue;
        }

        auto* anim = new QVariantAnimation(this);
        anim->setStartValue(current);
        anim->setEndValue(target);
        anim->setDuration(themeAnimation().fast);
        anim->setEasingCurve(themeAnimation().decelerate);
        connect(anim, &QVariantAnimation::valueChanged, this, [this, i](const QVariant& v) {
            m_dragOffsets[i] = v.toReal();
            viewport()->update();
        });
        connect(anim, &QVariantAnimation::finished, this, [this, i, target]() {
            m_dragOffsets[i] = target;
            if (auto* a = m_dragAnims.value(i)) {
                a->deleteLater();
                m_dragAnims.remove(i);
            }
        });
        m_dragAnims[i] = anim;
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    }
}

void ListView::clearDragAnimations() {
    for (auto it = m_dragAnims.begin(); it != m_dragAnims.end(); ++it) {
        if (it.value()) {
            it.value()->stop();
            it.value()->deleteLater();
        }
    }
    m_dragAnims.clear();
    m_dragOffsets.clear();
    viewport()->update();
}

} // namespace fluent::collections

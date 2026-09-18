#include "components/collections/FileListView.h"

#include <QAbstractItemModel>
#include <QAccessible>
#include <QAccessibleWidget>
#include <QApplication>
#include <QElapsedTimer>
#include <QHideEvent>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPointer>
#include <QResizeEvent>
#include <QSet>
#include <QStyledItemDelegate>
#include <QTextLayout>
#include <QTimer>
#include <QtMath>

#include <cmath>

#include "components/collections/private/CollectionItemState_p.h"
#include "components/foundation/MotionPolicy.h"
#include "design/IconCatalog.h"

namespace fluent::collections {

namespace {

constexpr int kRowHeight = 64;
constexpr int kPadding = 12;
constexpr int kIconSize = 20;
constexpr int kActionSize = 32;
constexpr int kStatusSize = 12;
constexpr int kStatusSpacing = 16;
constexpr int kTransitionMs = 160;

enum class FileAction { None, Remove, Retry };

FileListView::Status fileStatus(const QModelIndex& index)
{
    const int value = index.data(FileListView::StatusRole).toInt();
    return value == int(FileListView::Status::Uploading)  ? FileListView::Status::Uploading
           : value == int(FileListView::Status::Rejected) ? FileListView::Status::Rejected
                                                          : FileListView::Status::Ready;
}

qreal fileProgress(const QModelIndex& index)
{
    const qreal value = index.data(FileListView::ProgressRole).toDouble();
    return std::isfinite(value) ? qBound<qreal>(0.0, value, 1.0) : 0.0;
}

bool removable(const QModelIndex& index)
{
    const QVariant value = index.data(FileListView::RemovableRole);
    return !value.isValid() || value.toBool();
}

bool retryable(const QModelIndex& index)
{
    return fileStatus(index) == FileListView::Status::Rejected &&
           index.data(FileListView::RetryableRole).toBool();
}

bool commandEnabled(const FileListView* view, const QModelIndex& index)
{
    return view && view->isEnabled() && index.isValid() && index.model() == view->model() &&
           index.parent() == view->rootIndex() && (index.flags() & Qt::ItemIsEnabled);
}

QString statusText(const QModelIndex& index)
{
    switch (fileStatus(index)) {
    case FileListView::Status::Ready:
        return FileListView::tr("Ready");
    case FileListView::Status::Uploading:
        return FileListView::tr("Uploading, %1 percent").arg(qRound(fileProgress(index) * 100.0));
    case FileListView::Status::Rejected:
        return FileListView::tr("Rejected");
    }
    return {};
}

void invokeCommand(FileListView* view, const QModelIndex& index, FileAction action)
{
    if (!commandEnabled(view, index))
        return;
    if (action == FileAction::Remove && removable(index))
        emit view->removeRequested(index);
    else if (action == FileAction::Retry && retryable(index))
        emit view->retryRequested(index);
}

// Each paint measures and draws the same temporary layout, including CJK and
// bidirectional text. No text layout is retained between frames.
// zh_CN: 每轮绘制共用同一临时排版对象进行测量和绘制，兼容 CJK 和双向文字，
// 不跨帧保留文字排版。
qreal layoutText(QTextLayout& layout, qreal width, qreal lineHeight, Qt::LayoutDirection direction)
{
    if (layout.text().isEmpty())
        return 0.0;
    layout.setCacheEnabled(true);
    QTextOption option;
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    // Mirror row geometry and alignment, but let each paragraph determine its
    // own base direction. Forcing RTL reverses numeric English metadata such
    // as "3.6 / 5.8 MB"; forcing LTR also breaks Arabic-leading mixed text.
    // zh_CN: 行几何和对齐跟随布局镜像，段落方向由文字自身决定，避免英文数字
    // 说明在 RTL 下颠倒，也避免阿拉伯文混排被强制为 LTR。
    option.setTextDirection(Qt::LayoutDirectionAuto);
    option.setAlignment(direction == Qt::RightToLeft ? Qt::AlignRight : Qt::AlignLeft);
    layout.setTextOption(option);
    layout.beginLayout();
    qreal height = 0.0;
    while (true) {
        QTextLine line = layout.createLine();
        if (!line.isValid())
            break;
        line.setLineWidth(qMax<qreal>(1.0, width));
        const qreal extent = qMax(lineHeight, line.height());
        line.setPosition(QPointF(0.0, height + (extent - line.height()) / 2.0));
        height += extent;
    }
    layout.endLayout();
    return height;
}

struct RowGeometry {
    QRectF icon;
    QRectF text;
    QRectF remove;
    QRectF retry;
};

RowGeometry rowGeometry(const QRectF& rect, const QModelIndex& index, Qt::LayoutDirection direction)
{
    const qreal contentY = rect.top() + kPadding;
    RowGeometry result;
    result.icon = QRectF(rect.left() + kPadding, contentY + 10.0, kIconSize, kIconSize);
    qreal right = rect.right() - kPadding;
    if (removable(index)) {
        result.remove = QRectF(right - kActionSize, contentY + 4.0, kActionSize, kActionSize);
        right -= kActionSize + 8.0;
    }
    if (retryable(index)) {
        result.retry = QRectF(right - kActionSize, contentY + 4.0, kActionSize, kActionSize);
        right -= kActionSize + 8.0;
    }
    result.text = QRectF(result.icon.right() + 12.0, contentY,
                         qMax<qreal>(1.0, right - result.icon.right() - 12.0),
                         qMax<qreal>(0.0, rect.height() - 2 * kPadding));
    if (direction == Qt::RightToLeft) {
        auto mirror = [&rect](QRectF& part) {
            if (!part.isEmpty())
                part.moveLeft(rect.left() + rect.right() - part.right());
        };
        mirror(result.icon);
        mirror(result.text);
        mirror(result.remove);
        mirror(result.retry);
    }
    return result;
}

} // namespace

class FileListViewInteraction {
public:
    static void selectCurrent(FileListView* view, const QModelIndex& index)
    {
        QPointer<FileListView> guardedView(view);
        QPointer<QItemSelectionModel> selection(view->selectionModel());
        if (!selection || !commandEnabled(view, index))
            return;
        const auto command = view->selectionCommand(index, nullptr);
        if (!guardedView || !selection)
            return;

        // A direct currentChanged callback can delete the view. Qt's selection
        // model still emits currentRowChanged/currentColumnChanged afterwards,
        // and QAbstractItemView::setCurrentIndex writes its private state after
        // those signals. Keep only a view-owned selection model alive through
        // its call and bypass the unsafe outer Qt setter. Borrowed selection
        // models retain their parent and ownership throughout.
        // zh_CN: currentChanged 回调可能直接删除视图；Qt 的 selection model 此后
        // 仍会发出行列变化信号，外层 setCurrentIndex 还会写视图私有状态。因此仅
        // 暂时保护视图持有的 selection model，直接调用它，借用对象的父级和所有权
        // 始终保持不变。
        struct SelectionLifetime {
            QPointer<QItemSelectionModel> selection;
            QPointer<FileListView> owner;
            bool viewOwned = false;

            ~SelectionLifetime()
            {
                if (!viewOwned || !selection || selection->parent())
                    return;
                if (owner)
                    selection->setParent(owner);
                else
                    selection->deleteLater();
            }
        } lifetime{selection, guardedView, selection->parent() == view};
        if (lifetime.viewOwned)
            selection->setParent(nullptr);
        if (guardedView && selection)
            selection->setCurrentIndex(index, command);
    }
};

class FileListView::Private {
public:
    explicit Private(FileListView* view) : q(view)
    {
        clock.start();
        timer.setInterval(16);
        timer.setTimerType(Qt::PreciseTimer);
        QObject::connect(&timer, &QTimer::timeout, q, [this] { tick(); });
    }

    struct Transition {
        qreal from = 0.0;
        qreal to = 0.0;
        qint64 start = 0;
        int duration = 0;

        qreal value(qint64 now) const
        {
            const qreal t =
                duration > 0 ? qBound<qreal>(0.0, qreal(now - start) / duration, 1.0) : 1.0;
            const qreal eased = 1.0 - std::pow(1.0 - t, 3.0);
            return from + (to - from) * eased;
        }
        bool finished(qint64 now) const { return now - start >= duration; }
    };

    bool canAnimate() const
    {
        return animationEnabled && q->isVisible() && q->isEnabled() && q->isActiveWindow() &&
               MotionPolicy::instance().shouldAnimate();
    }

    QRect rowRect(const QModelIndex& index) const { return q->visualRect(index); }
    bool visible(const QModelIndex& index) const
    {
        return index.isValid() && index.model() == q->model() &&
               rowRect(index).intersects(q->viewport()->rect());
    }

    FileAction actionAt(const QModelIndex& index, const QPoint& point) const
    {
        if (!commandEnabled(q, index))
            return FileAction::None;
        const RowGeometry geometry = rowGeometry(rowRect(index), index, q->layoutDirection());
        if (geometry.remove.contains(point))
            return FileAction::Remove;
        if (geometry.retry.contains(point))
            return FileAction::Retry;
        return FileAction::None;
    }

    void setHovered(const QModelIndex& index, const QPoint& point)
    {
        const FileAction action = actionAt(index, point);
        if (hovered == index && hoveredAction == action)
            return;
        hoveredAction = action;
        if (hovered == index) {
            if (index.isValid())
                q->viewport()->update(rowRect(index));
            return;
        }
        const qint64 now = clock.elapsed();
        if (hovered.isValid()) {
            const qreal value = hoverTransitions.contains(hovered)
                                    ? hoverTransitions.value(hovered).value(now)
                                    : 1.0;
            hoverTransitions.insert(hovered, {value, 0.0, now, duration()});
        }
        hovered = index;
        if (hovered.isValid()) {
            const qreal value = hoverTransitions.contains(hovered)
                                    ? hoverTransitions.value(hovered).value(now)
                                    : 0.0;
            hoverTransitions.insert(hovered, {value, 1.0, now, duration()});
        }
        tick();
        if (!hoverTransitions.isEmpty())
            timer.start();
    }

    int duration() const
    {
        return canAnimate() ? MotionPolicy::instance().resolvedDuration(kTransitionMs) : 0;
    }

    qreal hoverOpacity(const QModelIndex& index) const
    {
        const auto found = hoverTransitions.constFind(index);
        return found != hoverTransitions.constEnd() ? found->value(clock.elapsed())
               : hovered == index                   ? 1.0
                                                    : 0.0;
    }

    qreal paintedProgress(const QModelIndex& index, qreal target)
    {
        const auto animated = progressTransitions.constFind(index);
        const qreal result =
            animated == progressTransitions.constEnd() ? target : animated->value(clock.elapsed());
        lastProgress.insert(index, result);
        return result;
    }

    void progressChanged(const QModelIndex& first, const QModelIndex& last)
    {
        const qint64 now = clock.elapsed();
        // Only entries painted in the current viewport are considered; a model
        // can report a million-row range without a million data() calls.
        // zh_CN: 仅遍历当前视口绘制过的行，大范围通知不会逐项读取整个模型。
        for (auto it = lastProgress.begin(); it != lastProgress.end();) {
            const QPersistentModelIndex index = it.key();
            if (!visible(index)) {
                progressTransitions.remove(index);
                it = lastProgress.erase(it);
                continue;
            }
            if (index.parent() == first.parent() && index.row() >= first.row() &&
                index.row() <= last.row()) {
                const qreal previous = progressTransitions.contains(index)
                                           ? progressTransitions.value(index).value(now)
                                           : it.value();
                const qreal target = fileProgress(index);
                if (!qFuzzyCompare(previous + 1.0, target + 1.0) && duration() > 0)
                    progressTransitions.insert(index, {previous, target, now, duration()});
                else
                    progressTransitions.remove(index);
                q->viewport()->update(rowRect(index));
            }
            ++it;
        }
        if (!progressTransitions.isEmpty())
            timer.start();
    }

    void tick()
    {
        const qint64 now = clock.elapsed();
        auto advance = [&](auto& transitions) {
            for (auto it = transitions.begin(); it != transitions.end();) {
                if (!visible(it.key())) {
                    it = transitions.erase(it);
                    continue;
                }
                q->viewport()->update(rowRect(it.key()));
                if (!canAnimate() || it->finished(now))
                    it = transitions.erase(it);
                else
                    ++it;
            }
        };
        advance(progressTransitions);
        advance(hoverTransitions);
        if (progressTransitions.isEmpty() && hoverTransitions.isEmpty())
            timer.stop();
    }

    void stopMotion()
    {
        timer.stop();
        progressTransitions.clear();
        hoverTransitions.clear();
        lastProgress.clear();
        q->viewport()->update();
    }

    void clear()
    {
        stopMotion();
        heights.clear();
        pendingHeights.clear();
        hovered = QPersistentModelIndex();
        pressed = QPersistentModelIndex();
        hoveredAction = pressedAction = FileAction::None;
    }

    void rememberHeight(const QModelIndex& index, int height);

    FileListView* q;
    FileListItemDelegate* delegate = nullptr;
    bool animationEnabled = true;
    bool heightDeliveryScheduled = false;
    QElapsedTimer clock;
    QTimer timer;
    QVector<QMetaObject::Connection> modelConnections;
    QHash<QPersistentModelIndex, int> heights;
    QSet<QPersistentModelIndex> pendingHeights;
    QHash<QPersistentModelIndex, qreal> lastProgress;
    QHash<QPersistentModelIndex, Transition> progressTransitions;
    QHash<QPersistentModelIndex, Transition> hoverTransitions;
    QPersistentModelIndex hovered;
    QPersistentModelIndex pressed;
    FileAction hoveredAction = FileAction::None;
    FileAction pressedAction = FileAction::None;
};

class FileListItemDelegate final : public QStyledItemDelegate {
public:
    explicit FileListItemDelegate(FileListView* view) : QStyledItemDelegate(view), m_view(view) {}

    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex& index) const override
    {
        // QListView may ask for every row's geometry. Do not query its text here;
        // paint() measures only visible content and coalesces changed heights.
        // zh_CN: QListView 可能查询全部行的几何；此处不读取文字，可见行才测量。
        const auto& heights = m_view->d->heights;
        const int height = heights.isEmpty() ? kRowHeight : heights.value(index, kRowHeight);
        return QSize(qMax(1, m_view->viewport()->width()), height);
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        if (!index.isValid())
            return;
        const auto& colors = m_view->themeColorsRef();
        const auto radius = m_view->themeRadius();
        auto* state = m_view->d.get();
        const RowGeometry geometry = rowGeometry(option.rect, index, m_view->layoutDirection());
        const QString name = index.data(Qt::DisplayRole).toString();
        QString metadata = index.data(FileListView::MetadataRole).toString();
        if (metadata.isEmpty())
            metadata = statusText(index);
        const auto status = fileStatus(index);
        const bool enabled = commandEnabled(m_view, index);
        const auto body = m_view->themeFont(Typography::FontRole::Body);
        const auto caption = m_view->themeFont(Typography::FontRole::Caption);
        QTextLayout nameLayout(name, body.toQFont());
        QTextLayout metadataLayout(metadata, caption.toQFont());
        const qreal nameHeight = layoutText(nameLayout, geometry.text.width(), body.lineHeight,
                                            m_view->layoutDirection());
        const qreal metadataWidth = qMax<qreal>(1.0, geometry.text.width() - kStatusSpacing);
        const qreal metadataHeight = layoutText(metadataLayout, metadataWidth, caption.lineHeight,
                                                m_view->layoutDirection());
        const qreal gap = metadataHeight > 0.0 && nameHeight > 0.0 ? 2.0 : 0.0;
        const bool rtl = m_view->layoutDirection() == Qt::RightToLeft;
        const qreal metadataY = geometry.text.top() + nameHeight + gap;
        const QPointF metadataOrigin(geometry.text.left() + (rtl ? 0 : kStatusSpacing), metadataY);
        const QRectF statusRect(rtl ? geometry.text.right() - kStatusSize : geometry.text.left(),
                                metadataY + (caption.lineHeight - kStatusSize) / 2.0, kStatusSize,
                                kStatusSize);
        const bool uploading = status == FileListView::Status::Uploading;
        const qreal contentHeight =
            2 * kPadding + nameHeight + gap + metadataHeight + (uploading ? 7.0 : 0.0);
        const int height = qMax(kRowHeight, 4 * qCeil(contentHeight / 4.0));
        state->rememberHeight(index, height);

        painter->save();
        painter->setClipRect(option.rect);
        painter->setRenderHint(QPainter::Antialiasing);
        const QRectF background = QRectF(option.rect).adjusted(0.0, 1.0, 0.0, -1.0);
        QStyle::State rowState = option.state;
        rowState.setFlag(QStyle::State_Enabled, enabled);
        const auto interaction = detail::collectionItemInteractionState(rowState);
        const bool animateHover = interaction == detail::CollectionItemInteractionState::Normal ||
                                  interaction == detail::CollectionItemInteractionState::Hovered;
        // Resolve shared collection colors, retaining a fading hover's endpoint
        // after Qt clears MouseOver. Disabled/selected/pressed states use the
        // same appearance as ListView and never inherit stale hover opacity.
        // zh_CN: 复用集合状态配色；Qt 清除 MouseOver 后仍保留悬停淡出的颜色终点。
        // 禁用、选中和按压状态与 ListView 一致，不继承残留的悬停透明度。
        if (animateHover)
            rowState |= QStyle::State_MouseOver;
        QColor fill = detail::collectionItemVisualStyle(rowState, colors).background;
        if (animateHover)
            fill.setAlphaF(fill.alphaF() * state->hoverOpacity(index));
        if (fill.alpha() > 0) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(fill);
            painter->drawRoundedRect(background, radius.control, radius.control);
        }

        painter->setPen(enabled ? colors.textSecondary : colors.textDisabled);
        Typography::Icons::paintGlyph(*painter, geometry.icon,
                                      QStringLiteral("ic_fluent_document_20_regular"), kIconSize);
        painter->setPen(enabled ? colors.textPrimary : colors.textDisabled);
        if (nameHeight > 0.0)
            nameLayout.draw(painter, geometry.text.topLeft());
        painter->setPen(!enabled                                   ? colors.textDisabled
                        : status == FileListView::Status::Rejected ? colors.systemCritical
                                                                   : colors.textSecondary);
        if (metadataHeight > 0.0)
            metadataLayout.draw(painter, metadataOrigin);

        if (uploading) {
            const QRectF track(geometry.text.left(),
                               geometry.text.top() + nameHeight + gap + metadataHeight + 4.0,
                               geometry.text.width(), 3.0);
            painter->setPen(Qt::NoPen);
            painter->setBrush(colors.controlAltSecondary);
            painter->drawRoundedRect(track, 1.5, 1.5);
            const qreal fraction = state->paintedProgress(index, fileProgress(index));
            QRectF progress = track;
            progress.setWidth(track.width() * fraction);
            if (rtl)
                progress.moveRight(track.right());
            painter->setBrush(enabled ? colors.accentDefault : colors.accentDisabled);
            if (progress.width() > 0.0)
                painter->drawRoundedRect(progress, 1.5, 1.5);
            painter->setPen(enabled ? colors.textSecondary : colors.textDisabled);
            Typography::Icons::paintGlyph(*painter, statusRect,
                                          QStringLiteral("ic_fluent_arrow_upload_16_regular"),
                                          kStatusSize);
        } else {
            const bool rejected = status == FileListView::Status::Rejected;
            painter->setPen(!enabled   ? colors.textDisabled
                            : rejected ? colors.systemCritical
                                       : colors.systemSuccess);
            Typography::Icons::paintGlyph(
                *painter, statusRect,
                rejected ? QStringLiteral("ic_fluent_error_circle_12_regular")
                         : QStringLiteral("ic_fluent_checkmark_circle_12_regular"),
                kStatusSize);
        }

        auto paintAction = [&](const QRectF& rect, FileAction action, const QString& glyph) {
            if (rect.isEmpty())
                return;
            const bool hovered = state->hovered == index && state->hoveredAction == action;
            const bool pressed = state->pressed == index && state->pressedAction == action;
            if (enabled && hovered) {
                painter->setPen(Qt::NoPen);
                painter->setBrush(pressed ? colors.subtleTertiary : colors.subtleSecondary);
                painter->drawRoundedRect(rect, radius.control, radius.control);
            }
            painter->setPen(enabled ? colors.textSecondary : colors.textDisabled);
            Typography::Icons::paintGlyph(*painter, rect, glyph, 16);
        };
        paintAction(geometry.remove, FileAction::Remove,
                    QStringLiteral("ic_fluent_dismiss_16_regular"));
        paintAction(geometry.retry, FileAction::Retry,
                    QStringLiteral("ic_fluent_arrow_clockwise_16_regular"));

        if ((option.state & QStyle::State_HasFocus) &&
            m_view->testAttribute(Qt::WA_KeyboardFocusChange)) {
            painter->setBrush(Qt::NoBrush);
            painter->setPen(QPen(colors.strokeFocusOuter, 2.0));
            painter->drawRoundedRect(background.adjusted(1.0, 1.0, -1.0, -1.0), radius.control,
                                     radius.control);
        }
        painter->restore();
    }

private:
    FileListView* m_view;
};

void FileListView::Private::rememberHeight(const QModelIndex& index, int height)
{
    if (heights.value(index, kRowHeight) == height)
        return;
    if (height == kRowHeight)
        heights.remove(index);
    else
        heights.insert(index, height);
    pendingHeights.insert(index);
    if (heightDeliveryScheduled)
        return;
    heightDeliveryScheduled = true;
    QTimer::singleShot(0, q, [this] {
        heightDeliveryScheduled = false;
        const auto pending = pendingHeights;
        pendingHeights.clear();
        for (const auto& index : pending) {
            if (index.isValid() && index.model() == q->model()) {
                // Qt queues a full layout for every sizeHintChanged signal.
                // Apply all heights measured in this frame with one layout.
                // zh_CN: Qt 为每个 sizeHintChanged 分别排入完整布局；同一帧
                // 测得的所有高度变化只安排一次布局。
                q->scheduleDelayedItemsLayout();
                break;
            }
        }
    });
}

namespace {

#if QT_CONFIG(accessibility)

class FileListRowAccessible final : public QAccessibleInterface, public QAccessibleActionInterface {
public:
    FileListRowAccessible(FileListView* view, const QModelIndex& index)
        : m_view(view), m_index(index)
    {}

    bool isValid() const override
    {
        return m_view && m_index.isValid() && m_index.model() == m_view->model() &&
               m_index.parent() == m_view->rootIndex();
    }
    QObject* object() const override { return nullptr; }
    QAccessibleInterface* parent() const override
    {
        return m_view ? QAccessible::queryAccessibleInterface(m_view) : nullptr;
    }
    QAccessibleInterface* child(int) const override { return nullptr; }
    QAccessibleInterface* childAt(int, int) const override { return nullptr; }
    int childCount() const override { return 0; }
    int indexOfChild(const QAccessibleInterface*) const override { return -1; }
    void setText(QAccessible::Text, const QString&) override {}
    QAccessible::Role role() const override { return QAccessible::ListItem; }
    QString text(QAccessible::Text type) const override
    {
        if (!isValid())
            return {};
        if (type == QAccessible::Name) {
            const QString name = m_index.data(Qt::AccessibleTextRole).toString();
            return name.isEmpty() ? m_index.data(Qt::DisplayRole).toString() : name;
        }
        if (type == QAccessible::Description) {
            const QString description = m_index.data(Qt::AccessibleDescriptionRole).toString();
            if (!description.isEmpty())
                return description;
            const QString metadata = m_index.data(FileListView::MetadataRole).toString();
            return metadata.isEmpty() ? statusText(m_index)
                                      : metadata + QStringLiteral(". ") + statusText(m_index);
        }
        return {};
    }
    QRect rect() const override
    {
        if (!isValid() || !m_view->isVisible())
            return {};
        const QRect row = static_cast<QAbstractItemView*>(m_view.data())->visualRect(m_index);
        return QRect(m_view->viewport()->mapToGlobal(row.topLeft()), row.size());
    }
    QAccessible::State state() const override
    {
        QAccessible::State result;
        result.invalid = !isValid();
        if (result.invalid)
            return result;
        result.disabled = !commandEnabled(m_view, m_index);
        result.focusable = !result.disabled;
        result.selectable = m_index.flags() & Qt::ItemIsSelectable;
        result.selected = m_view->selectionModel() && m_view->selectionModel()->isSelected(m_index);
        result.focused = m_view->hasFocus() && m_view->currentIndex() == m_index;
        result.invisible = !m_view->isVisible();
        const QRect local = static_cast<QAbstractItemView*>(m_view.data())->visualRect(m_index);
        result.offscreen = result.invisible || !local.intersects(m_view->viewport()->rect());
        result.readOnly = true;
        return result;
    }
    void* interface_cast(QAccessible::InterfaceType type) override
    {
        return type == QAccessible::ActionInterface ? static_cast<QAccessibleActionInterface*>(this)
                                                    : nullptr;
    }
    QStringList actionNames() const override
    {
        if (!isValid() || !commandEnabled(m_view, m_index))
            return {};
        QStringList result{QAccessibleActionInterface::setFocusAction()};
        if (removable(m_index))
            result.append(QStringLiteral("remove"));
        if (retryable(m_index))
            result.append(QStringLiteral("retry"));
        return result;
    }
    QString localizedActionName(const QString& name) const override
    {
        if (name == QStringLiteral("remove"))
            return FileListView::tr("Remove file");
        if (name == QStringLiteral("retry"))
            return FileListView::tr("Retry file");
        return QAccessibleActionInterface::localizedActionName(name);
    }
    QString localizedActionDescription(const QString& name) const override
    {
        if (name == QStringLiteral("remove"))
            return FileListView::tr("Requests removal of this file");
        if (name == QStringLiteral("retry"))
            return FileListView::tr("Requests another attempt for this file");
        return QAccessibleActionInterface::localizedActionDescription(name);
    }
    QStringList keyBindingsForAction(const QString& name) const override
    {
        if (name == QStringLiteral("remove"))
            return {QStringLiteral("Delete"), QStringLiteral("Backspace")};
        if (name == QStringLiteral("retry"))
            return {QStringLiteral("Ctrl+R")};
        return {};
    }
    void doAction(const QString& name) override
    {
        if (!isValid() || !commandEnabled(m_view, m_index))
            return;
        // Selection/focus callbacks may destroy this view and its adapter.
        // zh_CN: 选择和焦点回调可能销毁视图及其无障碍对象。
        QPointer<FileListView> view = m_view;
        const QPersistentModelIndex index = m_index;
        if (name == QAccessibleActionInterface::setFocusAction()) {
            FileListViewInteraction::selectCurrent(view, index);
            if (!view || !commandEnabled(view, index))
                return;
            view->scrollTo(index);
            if (view && commandEnabled(view, index))
                view->setFocus(Qt::OtherFocusReason);
        } else if (name == QStringLiteral("remove")) {
            invokeCommand(view, index, FileAction::Remove);
        } else if (name == QStringLiteral("retry")) {
            invokeCommand(view, index, FileAction::Retry);
        }
    }
    const QPersistentModelIndex& index() const { return m_index; }

private:
    QPointer<FileListView> m_view;
    QPersistentModelIndex m_index;
};

class FileListAccessible final : public QAccessibleWidget {
public:
    explicit FileListAccessible(FileListView* view) : QAccessibleWidget(view, QAccessible::List) {}
    ~FileListAccessible() override
    {
        disconnectModel();
        clearChildren();
    }
    FileListView* view() const { return static_cast<FileListView*>(widget()); }
    int childCount() const override
    {
        return view() && view()->model() ? view()->model()->rowCount(view()->rootIndex()) : 0;
    }
    QAccessibleInterface* child(int row) const override
    {
        if (row < 0 || row >= childCount())
            return nullptr;
        syncModel();
        const QModelIndex index = view()->model()->index(row, 0, view()->rootIndex());
        const auto found = m_children.constFind(index);
        if (found != m_children.constEnd()) {
            if (auto* child = QAccessible::accessibleInterface(found.value()))
                return child;
        }
        auto* result = new FileListRowAccessible(view(), index);
        m_children.insert(index, QAccessible::registerAccessibleInterface(result));
        return result;
    }
    int indexOfChild(const QAccessibleInterface* child) const override
    {
        const auto* row = dynamic_cast<const FileListRowAccessible*>(child);
        return row && row->isValid() && row->parent() == this ? row->index().row() : -1;
    }
    QAccessibleInterface* childAt(int x, int y) const override
    {
        if (!view())
            return nullptr;
        const QModelIndex index = view()->indexAt(view()->viewport()->mapFromGlobal(QPoint(x, y)));
        return index.isValid() ? child(index.row()) : nullptr;
    }
    QAccessibleInterface* focusChild() const override
    {
        return view() && view()->hasFocus() && view()->currentIndex().isValid()
                   ? child(view()->currentIndex().row())
                   : nullptr;
    }

private:
    void disconnectModel() const
    {
        for (const auto& connection : m_connections)
            QObject::disconnect(connection);
        m_connections.clear();
    }

    void clearChildren() const
    {
        const auto children = m_children;
        m_children.clear();
        for (auto id : children)
            QAccessible::deleteAccessibleInterface(id);
    }

    void syncModel() const
    {
        if (m_model == view()->model() && m_root == view()->rootIndex())
            return;
        disconnectModel();
        clearChildren();
        m_model = view()->model();
        m_root = view()->rootIndex();
        if (!m_model)
            return;
        m_connections.append(QObject::connect(m_model, &QAbstractItemModel::modelReset, view(),
                                              [this] { clearChildren(); }));
        m_connections.append(
            QObject::connect(m_model, &QAbstractItemModel::rowsRemoved, view(), [this] {
                for (auto it = m_children.begin(); it != m_children.end();) {
                    if (!it.key().isValid()) {
                        QAccessible::deleteAccessibleInterface(it.value());
                        it = m_children.erase(it);
                    } else {
                        ++it;
                    }
                }
            }));
    }

    mutable QPointer<QAbstractItemModel> m_model;
    mutable QPersistentModelIndex m_root;
    mutable QVector<QMetaObject::Connection> m_connections;
    mutable QHash<QPersistentModelIndex, QAccessible::Id> m_children;
};

QAccessibleInterface* fileListAccessibilityFactory(const QString&, QObject* object)
{
    auto* view = qobject_cast<FileListView*>(object);
    return view ? new FileListAccessible(view) : nullptr;
}

#endif

void ensureAccessibility()
{
#if QT_CONFIG(accessibility)
    static const bool installed = [] {
        QAccessible::installFactory(fileListAccessibilityFactory);
        return true;
    }();
    Q_UNUSED(installed)
#endif
}

} // namespace

FileListView::FileListView(QWidget* parent) : ListView(parent), d(std::make_unique<Private>(this))
{
    ensureAccessibility();
    d->delegate = new FileListItemDelegate(this);
    setItemDelegate(d->delegate);
    setBorderVisible(false);
    setBackgroundVisible(false);
    setSelectionIndicatorVisible(false);
    setSpacing(0);
    setLayoutMode(QListView::Batched);
    setBatchSize(256);
    setResizeMode(QListView::Adjust);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setMouseTracking(true);
    setAccessibleName(tr("Files"));
    setAccessibleDescription(
        tr("Use arrow keys to select a file. Delete removes it; Control+R retries it."));
    connect(&MotionPolicy::instance(), &MotionPolicy::modeChanged, this,
            [this] { d->stopMotion(); });
    connect(qApp, &QApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
        if (state != Qt::ApplicationActive)
            d->stopMotion();
    });
}

FileListView::~FileListView() = default;

void FileListView::setModel(QAbstractItemModel* model)
{
    for (const auto& connection : d->modelConnections)
        disconnect(connection);
    d->modelConnections.clear();
    d->clear();
    ListView::setModel(model);
    if (!model)
        return;
    d->modelConnections.append(
        connect(model, &QAbstractItemModel::modelAboutToBeReset, this, [this] { d->clear(); }));
    d->modelConnections.append(connect(model, &QObject::destroyed, this, [this] { d->clear(); }));
    d->modelConnections.append(connect(model, &QAbstractItemModel::rowsRemoved, this, [this] {
        for (auto it = d->heights.begin(); it != d->heights.end();) {
            if (!it.key().isValid())
                it = d->heights.erase(it);
            else
                ++it;
        }
        d->tick();
    }));
    d->modelConnections.append(
        connect(model, &QAbstractItemModel::layoutChanged, this, [this] { d->tick(); }));
}

bool FileListView::isAnimationEnabled() const
{
    return d->animationEnabled;
}

void FileListView::setAnimationEnabled(bool enabled)
{
    if (d->animationEnabled == enabled)
        return;
    d->animationEnabled = enabled;
    if (!enabled)
        d->stopMotion();
    emit animationEnabledChanged(enabled);
}

void FileListView::dataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight,
                               const QVector<int>& roles)
{
    if (!d) {
        ListView::dataChanged(topLeft, bottomRight, roles);
        return;
    }
    d->progressChanged(topLeft, bottomRight);
    // Preserve known geometry until visible content actually needs a different
    // height. Uploaders commonly change byte-count metadata with each progress
    // update; these must not invalidate layout for the entire collection.
    // zh_CN: 保留已知几何，仅在可见内容实测高度改变时重新布局。上传过程常同时
    // 更新字节数说明与进度，不能因此每帧让整表几何失效。
    QAbstractItemView::dataChanged(topLeft, bottomRight, roles);
}

void FileListView::mousePressEvent(QMouseEvent* event)
{
    const QModelIndex index = indexAt(event->pos());
    const FileAction action = d->actionAt(index, event->pos());
    if (event->button() == Qt::LeftButton && action != FileAction::None) {
        d->pressed = index;
        d->pressedAction = action;
        QPointer<FileListView> guard(this);
        const QPersistentModelIndex pressed = index;
        FileListViewInteraction::selectCurrent(this, pressed);
        if (!guard)
            return;
        if (!commandEnabled(this, pressed)) {
            d->pressed = QPersistentModelIndex();
            d->pressedAction = FileAction::None;
            event->accept();
            return;
        }
        setFocus(Qt::MouseFocusReason);
        if (!guard)
            return;
        if (commandEnabled(this, pressed))
            viewport()->update(visualRect(pressed));
        event->accept();
        return;
    }
    ListView::mousePressEvent(event);
}

void FileListView::mouseMoveEvent(QMouseEvent* event)
{
    const QModelIndex index = indexAt(event->pos());
    d->setHovered(index, event->pos());
    viewport()->setCursor(d->actionAt(index, event->pos()) == FileAction::None
                              ? Qt::ArrowCursor
                              : Qt::PointingHandCursor);
    if (d->pressedAction != FileAction::None) {
        event->accept();
        return;
    }
    ListView::mouseMoveEvent(event);
}

void FileListView::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && d->pressedAction != FileAction::None) {
        const QPersistentModelIndex pressed = d->pressed;
        const FileAction action = d->pressedAction;
        d->pressed = QPersistentModelIndex();
        d->pressedAction = FileAction::None;
        viewport()->update();
        if (pressed.isValid() && indexAt(event->pos()) == pressed &&
            d->actionAt(pressed, event->pos()) == action)
            invokeCommand(this, pressed, action);
        event->accept();
        return;
    }
    ListView::mouseReleaseEvent(event);
}

void FileListView::keyPressEvent(QKeyEvent* event)
{
    if (!event->isAutoRepeat() && event->modifiers() == Qt::NoModifier &&
        (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace)) {
        invokeCommand(this, currentIndex(), FileAction::Remove);
        event->accept();
        return;
    }
    if (!event->isAutoRepeat() && event->key() == Qt::Key_R &&
        event->modifiers() == Qt::ControlModifier) {
        invokeCommand(this, currentIndex(), FileAction::Retry);
        event->accept();
        return;
    }
    ListView::keyPressEvent(event);
}

void FileListView::leaveEvent(QEvent* event)
{
    d->setHovered(QModelIndex(), QPoint());
    viewport()->unsetCursor();
    ListView::leaveEvent(event);
}

void FileListView::resizeEvent(QResizeEvent* event)
{
    if (event->size().width() != event->oldSize().width()) {
        d->heights.clear();
        d->pendingHeights.clear();
        d->stopMotion();
    }
    ListView::resizeEvent(event);
}

void FileListView::hideEvent(QHideEvent* event)
{
    d->hovered = QPersistentModelIndex();
    d->pressed = QPersistentModelIndex();
    d->pressedAction = FileAction::None;
    d->stopMotion();
    ListView::hideEvent(event);
}

bool FileListView::event(QEvent* event)
{
    if (d && (event->type() == QEvent::WindowDeactivate || event->type() == QEvent::EnabledChange))
        d->stopMotion();
    return ListView::event(event);
}

void FileListView::scrollContentsBy(int dx, int dy)
{
    ListView::scrollContentsBy(dx, dy);
    for (auto it = d->lastProgress.begin(); it != d->lastProgress.end();) {
        if (!d->visible(it.key()))
            it = d->lastProgress.erase(it);
        else
            ++it;
    }
    d->tick();
}

void FileListView::onThemeUpdated()
{
    ListView::onThemeUpdated();
    if (!d)
        return;
    d->heights.clear();
    d->pendingHeights.clear();
    d->stopMotion();
    scheduleDelayedItemsLayout();
}

} // namespace fluent::collections

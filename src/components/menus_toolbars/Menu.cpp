#include "compatibility/TextPaintCompat.h"
#include "compatibility/InputEventCompat.h"
#include "Menu.h"
#include "compatibility/private/WidgetPlatformCompat_p.h"

#include <QActionEvent>
#include <QApplication>
#include <QEasingCurve>
#include <QEvent>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QRegion>
#include <QResizeEvent>
#include <QScreen>
#include <QShowEvent>
#include <QTimer>
#include <QVariantAnimation>
#include "compatibility/private/PointerEventCompat_p.h"
#include "compatibility/private/RuntimePlatformCapabilities_p.h"
#include "components/foundation/overlay/OverlayGeometry.h"
#include "components/foundation/private/MotionPolicy_p.h"
#include "components/menus_toolbars/private/MenuSurface_p.h"
#include "design/MenuTokens_p.h"
#include "design/Typography.h"

namespace fluent::menus_toolbars {

namespace {
constexpr char kEntranceAnimationName[] = "fluentMenuEntranceAnimation";
constexpr char kRestoreNativeMenuAnimationsProperty[] = "_fluent_restoreNativeMenuAnimations";

QString menuLabelText(const QString& text)
{
    const int tabIndex = text.indexOf(QLatin1Char('\t'));
    QString label = tabIndex >= 0 ? text.left(tabIndex) : text;
    label.replace(QStringLiteral("&&"), QStringLiteral("\u0001"));
    label.remove(QLatin1Char('&'));
    label.replace(QStringLiteral("\u0001"), QStringLiteral("&"));
    return label;
}

QString embeddedShortcutText(const QString& text)
{
    const int tabIndex = text.indexOf(QLatin1Char('\t'));
    return tabIndex >= 0 ? text.mid(tabIndex + 1).trimmed() : QString();
}

int maxLabelWidth(const FluentMenu* menu, const QFontMetrics& fontMetrics)
{
    int result = 0;
    for (QAction* action : menu->actions()) {
        if (!action || !action->isVisible() || action->isSeparator())
            continue;
        const QString text = menuLabelText(action->text());
        result = qMax(result, qMax(fontMetrics.horizontalAdvance(text), fontMetrics.boundingRect(text).width()));
    }
    return result > 0 ? result + 2 : 0;
}

int maxShortcutWidth(const FluentMenu* menu, const QFontMetrics& fontMetrics)
{
    int result = 0;
    for (QAction* action : menu->actions()) {
        if (!action || !action->isVisible() || action->isSeparator())
            continue;
        const QString text = menu->shortcutTextForAction(action);
        result = qMax(result, qMax(fontMetrics.horizontalAdvance(text), fontMetrics.boundingRect(text).width()));
    }
    // Pad both sides: Windows font backends can paint antialiased bearings beyond horizontalAdvance() and boundingRect().
    return result > 0 ? result + 6 : 0;
}

bool hasSubmenuAction(const FluentMenu* menu)
{
    for (QAction* action : menu->actions()) {
        if (action && action->isVisible() && !action->isSeparator() && action->menu())
            return true;
    }
    return false;
}

bool hasLeadingAction(const FluentMenu* menu)
{
    for (QAction* action : menu->actions()) {
        if (!action || !action->isVisible() || action->isSeparator())
            continue;
        if (action->isCheckable() || !action->icon().isNull())
            return true;
    }
    return false;
}

QMenu* visibleSubmenuAt(const QMenu* menu, const QPoint& globalPosition)
{
    for (QAction* action : menu->actions()) {
        QMenu* submenu = action->menu();
        if (submenu && submenu != menu && submenu->isVisible() && submenu->frameGeometry().contains(globalPosition))
            return submenu;
    }
    return nullptr;
}

FluentMenu* rootMenuFor(FluentMenu* menu)
{
    if (!menu)
        return nullptr;

    auto* root = menu;
    for (QWidget* parent = menu->parentWidget(); parent; parent = parent->parentWidget()) {
        auto* parentMenu = qobject_cast<FluentMenu*>(parent);
        if (!parentMenu)
            break;
        root = parentMenu;
    }
    return root;
}

void appendVisibleMenuChain(QVector<FluentMenu*>& menus, FluentMenu* menu)
{
    if (!menu || !menu->isVisible() || menus.contains(menu))
        return;

    menus.append(menu);
    for (QAction* action : menu->actions()) {
        auto* childMenu = action ? qobject_cast<FluentMenu*>(action->menu()) : nullptr;
        appendVisibleMenuChain(menus, childMenu);
    }
}
} // namespace

FluentMenuItem::FluentMenuItem(const QString& text, QObject* parent) : QWidgetAction(parent)
{
    setText(text);
    setFont(themeFont(m_fontStyle).toQFont());
}

void FluentMenuItem::setFontStyle(Typography::FontRole role)
{
    if (m_fontStyle == role)
        return;
    m_fontStyle = role;
    onThemeUpdated();
    emit fontStyleChanged();
}

void FluentMenuItem::onThemeUpdated()
{
    setFont(themeFont(m_fontStyle).toQFont());
}

FluentMenu::FluentMenu(const QString& title, QWidget* parent) : QMenu(title, parent)
{
    // Disable system drop shadow; FluentMenu paints its own rounded card and shadow.
    setWindowFlags(
        (windowFlags() & ~Qt::WindowType_Mask) | Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    m_translucentSurface = compatibility::detail::runtimePlatformCapabilities().translucentPopupSurfaces;
    m_shadowSize = m_translucentSurface ? ::Spacing::Standard : 0;
    setAttribute(Qt::WA_TranslucentBackground, m_translucentSurface);
    setAttribute(Qt::WA_OpaquePaintEvent, !m_translucentSurface);
    setAttribute(Qt::WA_Hover);
    setAutoFillBackground(false);
    setContentsMargins(m_shadowSize, m_shadowSize, m_shadowSize, m_shadowSize);

    connect(this, &QMenu::aboutToShow, this, [this]() {
        // The invoking menu can differ from QObject ownership when menus are shared.
        auto* parent = qobject_cast<QMenu*>(QApplication::activePopupWidget());
        m_cascadeParent = parent && parent != this && parent->activeAction() && parent->activeAction()->menu() == this
            ? parent
            : nullptr;
        if (m_cascadeParent)
            setLayoutDirection(m_cascadeParent->layoutDirection());
        if (!QApplication::isEffectEnabled(Qt::UI_AnimateMenu))
            return;

        // QMenu picks its global platform menu animation right after aboutToShow();
        // suppress it until showEvent() so our paint-only reveal runs instead.
        setProperty(kRestoreNativeMenuAnimationsProperty, true);
        QApplication::setEffectEnabled(Qt::UI_AnimateMenu, false);

        // Empty menus or aboutToShow() deletion can skip showEvent(), so always queue a restore.
        const QPointer<FluentMenu> guard(this);
        QTimer::singleShot(0, qApp, [guard]() {
            if (guard && !guard->property(kRestoreNativeMenuAnimationsProperty).toBool())
                return;
            if (guard)
                guard->setProperty(kRestoreNativeMenuAnimationsProperty, false);
            QApplication::setEffectEnabled(Qt::UI_AnimateMenu, true);
        });
    });

    setFont(themeFont(m_fontStyle).toQFont());
    onThemeUpdated();
}

void FluentMenu::setFontStyle(Typography::FontRole role)
{
    if (m_fontStyle == role)
        return;
    m_fontStyle = role;
    onThemeUpdated();
    emit fontStyleChanged();
}

void FluentMenu::onThemeUpdated()
{
    // Clear the previous auto width floor so QMenu recomputes action geometry for the new font.
    if (minimumWidth() == m_autoMinimumWidth && m_autoMinimumWidth > 0) {
        setMinimumWidth(0);
        m_autoMinimumWidth = 0;
    }

    const auto& s = themeSpacing();
    const int vPadding = s.gap.tight;

    // Sync the widget font because QMenu derives actionGeometry() heights from it.
    setFont(themeFont(m_fontStyle).toQFont());

    // Reserve shadow and inner padding in contentsMargins so QMenu::sizeHint() includes them.
    setContentsMargins(m_shadowSize, m_shadowSize + vPadding, m_shadowSize, m_shadowSize + vPadding);
    setItemLayoutMetrics(s.padding.listItemV, s.gap.normal + 1);
    syncAutoMinimumWidth();
    updateSurfaceMask();
    updateGeometry();
    update();
}

void FluentMenu::syncAutoMinimumWidth()
{
    // Reset any prior auto floor so font shrinkage can narrow the menu, while keeping caller minimums.
    if (minimumWidth() == m_autoMinimumWidth)
        setMinimumWidth(0);
    const int targetWidth = sizeHint().width();
    if (minimumWidth() <= m_autoMinimumWidth) {
        m_autoMinimumWidth = targetWidth;
        setMinimumWidth(targetWidth);
    }
}

void FluentMenu::setItemLayoutMetrics(int verticalPadding, int separatorHeight)
{
    m_itemVerticalPadding = qMax(0, verticalPadding);
    m_separatorHeight = qMax(1, separatorHeight);
    setStyleSheet(QStringLiteral(
        "QMenu { background-color: transparent; border: 0px; padding: 0px; }"
        "QMenu::item { background-color: transparent; padding: %1px 0px; "
        "margin: 0px; }"
        "QMenu::separator { height: %2px; }")
            .arg(m_itemVerticalPadding)
            .arg(m_separatorHeight));
}

void FluentMenu::actionEvent(QActionEvent* event)
{
    const bool trackActionMetrics = event->type() == QEvent::ActionAdded || event->type() == QEvent::ActionRemoved
        || event->type() == QEvent::ActionChanged;
    if (trackActionMetrics && minimumWidth() == m_autoMinimumWidth && m_autoMinimumWidth > 0) {
        setMinimumWidth(0);
        m_autoMinimumWidth = 0;
    }
    QMenu::actionEvent(event);
    if (event->type() == QEvent::ActionAdded && event->action() && !event->action()->isSeparator()) {
        QAction* action = event->action();
        QObject::disconnect(m_actionTriggerConnections.take(action));
        m_actionTriggerConnections.insert(action,
            QObject::connect(action, &QAction::triggered, this, [this, action = QPointer<QAction>(action)]() {
                if (!action || action->isSeparator() || action->menu())
                    return;
                QPointer<FluentMenu> guard(this);
                for (QPointer<QObject> obj = this; obj;) {
                    const QPointer<QObject> parent = obj->parent();
                    if (auto* menu = qobject_cast<QMenu*>(obj.data()); menu && menu->isWindow())
                        menu->hide();
                    if (!guard)
                        return;
                    obj = parent;
                }
            }));
    } else if (event->type() == QEvent::ActionRemoved) {
        QObject::disconnect(m_actionTriggerConnections.take(event->action()));
    }

    if (event->type() == QEvent::ActionAdded || event->type() == QEvent::ActionRemoved
        || event->type() == QEvent::ActionChanged) {
        syncAutoMinimumWidth();
        updateGeometry();
        update();
    }
}

void FluentMenu::keyPressEvent(QKeyEvent* event)
{
    if (event && event->key() != Qt::Key_unknown) {
        const QKeySequence pressed(fluentKeySequence(event));
        for (QAction* action : actions()) {
            if (!action || action->isSeparator() || action->menu() || !action->isEnabled() || !action->isVisible())
                continue;
            if (action->shortcut().isEmpty() || action->shortcut().matches(pressed) != QKeySequence::ExactMatch)
                continue;

            setActiveAction(action);
            event->accept();
            action->trigger();
            return;
        }
    }

    if (!isWindow() && event) {
        if (event->key() == Qt::Key_Escape) {
            setActiveAction(nullptr);
            update();
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter || event->key() == Qt::Key_Space) {
            QAction* active = activeAction();
            if (active && !active->menu() && !active->isSeparator() && active->isEnabled()) {
                event->accept();
                // Triggering may synchronously destroy this menu or its owner.
                active->trigger();
                return;
            }
        }
    }

    QMenu::keyPressEvent(event);
}

void FluentMenu::enterEvent(FluentEnterEvent* event)
{
    // Translucent shadows overlap, so native hit-testing can briefly deliver Enter to
    // the parent after the cursor enters a submenu. Forward it to the child so Qt
    // updates mouse tracking and cancels ancestor close timers.
    if (const auto position = compatibility::detail::enterEventGlobalPosition(event)) {
        if (QPointer<QMenu> submenu = visibleSubmenuAt(this, position->toPoint())) {
            QEvent leave(QEvent::Leave);
            QMenu::leaveEvent(&leave);
            if (submenu)
                compatibility::detail::sendEnterEvent(submenu, event);
            return;
        }
    }
    QMenu::enterEvent(event);
}

void FluentMenu::mouseMoveEvent(QMouseEvent* event)
{
    if (QMenu* submenu = visibleSubmenuAt(this, fluentMouseGlobalPos(event))) {
        // Native routing can also misdeliver the follow-up move to the parent.
        compatibility::detail::sendMouseEvent(submenu, event);
        return;
    }
    QMenu::mouseMoveEvent(event);
}

void FluentMenu::mousePressEvent(QMouseEvent* event)
{
    if (!isWindow()) {
        QAction* action = event ? actionAt(event->pos()) : nullptr;
        if (!action || action->isSeparator() || !action->isEnabled()) {
            setActiveAction(nullptr);
            update();
            if (event)
                event->accept();
            return;
        }
        if (!action->menu()) {
            setActiveAction(action);
            update();
            if (event)
                event->accept();
            return;
        }
    }

    QPointer<FluentMenu> guard(this);
    QMenu::mousePressEvent(event);
    if (!guard)
        return;
    guard->normalizePopupLayering();
    QTimer::singleShot(0, guard, [guard]() {
        if (guard)
            guard->normalizePopupLayering();
    });
}

void FluentMenu::mouseReleaseEvent(QMouseEvent* event)
{
    QPointer<FluentMenu> guard(this);
    if (!isWindow()) {
        QAction* action = event ? actionAt(event->pos()) : nullptr;
        if (!action || !action->menu()) {
            if (event && event->button() == Qt::LeftButton && action && !action->isSeparator() && action->isEnabled()) {
                setActiveAction(action);
                action->trigger();
            }
            if (guard)
                guard->update();
            if (event)
                event->accept();
            return;
        }
    }

    QMenu::mouseReleaseEvent(event);
    if (!guard)
        return;
    guard->normalizePopupLayering();
    QTimer::singleShot(0, guard, [guard]() {
        if (guard)
            guard->normalizePopupLayering();
    });
}

QString FluentMenu::shortcutTextForAction(QAction* action) const
{
    if (!action || action->isSeparator())
        return QString();

    const QVariant explicitText = action->property("shortcutText");
    if (explicitText.isValid() && !explicitText.toString().trimmed().isEmpty())
        return explicitText.toString().trimmed();

    if (!action->shortcut().isEmpty())
        return action->shortcut().toString(QKeySequence::NativeText);

    return embeddedShortcutText(action->text());
}

QRect FluentMenu::itemShortcutGeometry(QAction* action) const
{
    if (!action || actionGeometry(action).isEmpty() || shortcutTextForAction(action).isEmpty())
        return QRect();

    const QFontMetrics fontMetrics(font());
    const int shortcutColumn = maxShortcutWidth(this, fontMetrics);
    if (shortcutColumn <= 0)
        return QRect();

    const auto& spacing = themeSpacing();
    const int trailingColumn = hasSubmenuAction(this) ? spacing.controlHeight.small : ::Spacing::Small;
    QRect rect = actionGeometry(action);
    rect.setLeft(m_shadowSize);
    rect.setWidth(width() - 2 * m_shadowSize);
    const int textPadding = spacing.padding.controlH;
    const int right = rect.right() - textPadding - trailingColumn;
    return QRect(qMax(rect.left(), right - shortcutColumn + 1), rect.top(), shortcutColumn, rect.height());
}

QRect FluentMenu::itemSubmenuIndicatorGeometry(QAction* action) const
{
    if (!action || !action->menu() || actionGeometry(action).isEmpty())
        return QRect();

    QRect rect = actionGeometry(action);
    rect.setLeft(m_shadowSize);
    rect.setWidth(width() - 2 * m_shadowSize);
    const auto& spacing = themeSpacing();
    const int side = spacing.controlHeight.small;
    const int textPadding = spacing.padding.controlH;
    return QRect(rect.right() - textPadding - side + 1, rect.top() + (rect.height() - side) / 2, side, side);
}

void FluentMenu::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    const auto& colors = themeColorsRef();

    // Clear alpha-capable popups to transparent; opaque fallback runtimes fill the full Fluent layer.
    p.setCompositionMode(QPainter::CompositionMode_Source);
    p.fillRect(rect(), m_translucentSurface ? Qt::transparent : colors.bgLayer);
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    // Fade only in paintEvent: QGraphicsOpacityEffect on a native QMenu popup can leave
    // Wayland QMenu::exec() invisible while its modal loop still blocks input.
    p.setOpacity(m_translucentSurface ? qBound<qreal>(0.0, m_revealProgress, 1.0) : 1.0);

    const auto& spacing = themeSpacing();
    const auto& radius = themeRadius();

    QRect itemsRect;
    for (QAction* action : actions()) {
        if (action->isVisible()) {
            itemsRect |= actionGeometry(action);
        }
    }

    if (itemsRect.isEmpty())
        return;

    QRect contentRect = rect().adjusted(m_shadowSize, m_shadowSize, -m_shadowSize, -m_shadowSize);

    if (m_translucentSurface && m_revealProgress < 1.0)
        p.setClipRect(QRectF(0, 0, width(), height() * m_revealProgress));

    detail::paintMenuSurface(p, contentRect, *this, m_translucentSurface);
    int r = radius.overlay;
    p.save();
    QPainterPath clipPath;
    clipPath.addRoundedRect(contentRect, r, r);
    p.setClipPath(clipPath);

    const int plateLeft = contentRect.left();
    const int plateWidth = contentRect.width();
    const int itemInset = spacing.gap.tight;
    const int textPadding = spacing.padding.controlH;
    const int leadingColumn = hasLeadingAction(this) ? spacing.controlHeight.small : 0;
    const QFontMetrics fontMetrics(font());
    const int shortcutColumn = maxShortcutWidth(this, fontMetrics);
    const int trailingColumn = hasSubmenuAction(this) ? spacing.controlHeight.small : ::Spacing::Small;
    const int shortcutGap = spacing.gap.section;

    // Reset font explicitly after the surface/shadow pass.
    p.setFont(font());

    for (QAction* action : actions()) {
        if (!action->isVisible())
            continue;

        QRect itemRect = actionGeometry(action);
        // Align horizontal span to the card bounds because actionGeometry() may omit shadow margins.
        itemRect.setLeft(plateLeft);
        itemRect.setWidth(plateWidth);

        if (!contentRect.intersects(itemRect))
            continue;

        if (action->isSeparator()) {
            p.setPen(colors.strokeDivider);
            int y = itemRect.center().y();
            p.drawLine(itemRect.left() + itemInset, y, itemRect.right() - itemInset, y);
            continue;
        }

        bool isEnabled = this->isEnabled() && action->isEnabled();
        bool isActive = (action == activeAction());
        const bool highlighted = isEnabled && isActive;
        const QColor bg = highlighted ? colors.subtleSecondary : QColor(Qt::transparent);

        if (bg.isValid() && bg.alpha() > 0) {
            QRectF bgRect = itemRect.adjusted(itemInset, 1, -itemInset, -1);
            p.setPen(Qt::NoPen);
            p.setBrush(bg);
            p.drawRoundedRect(bgRect, radius.control, radius.control);
        }
        const QColor primaryText = isEnabled ? colors.textPrimary : colors.textDisabled;
        const QColor secondaryText = isEnabled ? colors.textSecondary : colors.textDisabled;

        if (leadingColumn > 0) {
            // Center icon by extent difference to avoid integer QRect center bias on even sizes.
            const int iconSide = qMin(Typography::IconSize::Standard, leadingColumn);
            const QRect iconRect(itemRect.left() + textPadding, itemRect.top() + (itemRect.height() - iconSide) / 2,
                iconSide, iconSide);
            if (action->isCheckable() && action->isChecked()) {
                p.setPen(primaryText);
                detail::paintMenuGlyph(p, QRectF(iconRect), Typography::Icons::CheckMark,
                    Typography::IconSize::Compact);
            } else if (!action->icon().isNull()) {
                const QIcon::Mode mode = !isEnabled ? QIcon::Disabled : (isActive ? QIcon::Active : QIcon::Normal);
                const QIcon::State state = action->isChecked() ? QIcon::On : QIcon::Off;
                action->icon().paint(&p, iconRect, Qt::AlignCenter, mode, state);
            }
        }

        const QString shortcutText = shortcutTextForAction(action);
        const QRect shortcutRect = itemShortcutGeometry(action);
        const int shortcutReserve = shortcutColumn > 0 ? shortcutColumn + shortcutGap : 0;
        const int textRight = itemRect.right() - textPadding - trailingColumn - shortcutReserve;
        const int textLeft = itemRect.left() + textPadding + leadingColumn;
        const qreal textBaseline = detail::menuTextBaseline(p, QRectF(itemRect));

        p.setPen(primaryText);
        // Draw at a baseline point rather than a tight QRect so Windows antialiased edge glyphs are not clipped.
        if (textLeft <= textRight)
            p.drawText(QPointF(textLeft, textBaseline), menuLabelText(action->text()));

        if (!shortcutRect.isEmpty()) {
            p.setPen(secondaryText);
            constexpr int shortcutInkInset = 6;
            const int shortcutX = shortcutRect.right() - shortcutInkInset - fontMetrics.horizontalAdvance(shortcutText)
                + 1;
            p.drawText(QPointF(shortcutX, textBaseline), shortcutText);
        }

        if (action->menu()) {
            const QRect arrowRect = itemSubmenuIndicatorGeometry(action);
            p.setPen(secondaryText);
            detail::paintMenuGlyph(p, QRectF(arrowRect), Typography::Icons::ChevronRightMed,
                Typography::IconSize::Compact);
        }
    }
    p.restore();
}

void FluentMenu::resizeEvent(QResizeEvent* event)
{
    QMenu::resizeEvent(event);
    updateSurfaceMask();
}

void FluentMenu::updateSurfaceMask()
{
    if (m_translucentSurface || rect().isEmpty()) {
        // Only clear our own opaque-fallback mask so platform masks (e.g. macOS) stay intact.
        if (m_surfaceMaskApplied) {
            clearMask();
            m_surfaceMaskApplied = false;
        }
        return;
    }

    // Opaque fallback popups (e.g. WASM) need a rounded QRegion mask so the full-rect clear
    // doesn't bleed past rounded corners; translucent desktop popups keep alpha-antialiased edges.
    const QRegion surfaceMask = ::fluent::overlay::roundedRectRegion(rect(), themeRadius().overlay);
    if (mask() != surfaceMask)
        setMask(surfaceMask);
    m_surfaceMaskApplied = true;
}

QSize FluentMenu::sizeHint() const
{
    QSize base = QMenu::sizeHint();
    const QFontMetrics fontMetrics(font());
    const auto& spacing = themeSpacing();

    const int shortcutTextWidth = maxShortcutWidth(this, fontMetrics);
    const int shortcutGap = spacing.gap.section;
    const int shortcutColumn = shortcutTextWidth > 0 ? shortcutTextWidth + shortcutGap : 0;
    const int trailingColumn = hasSubmenuAction(this) ? spacing.controlHeight.small : ::Spacing::Small;
    const int leadingColumn = hasLeadingAction(this) ? spacing.controlHeight.small : 0;
    const int textPadding = spacing.padding.controlH;
    const int contentWidth = spacing.gap.tight * 2 + textPadding * 2 + leadingColumn + maxLabelWidth(this, fontMetrics)
        + shortcutColumn + trailingColumn;

    // Qt WASM QMenu::sizeHint() can under-report multi-action popups as one row;
    // compute a lower bound from visible actions while letting native platforms keep larger hints.
    int contentHeight = contentsMargins().top() + contentsMargins().bottom();
    const int itemContentHeight = qMax(fontMetrics.height(), Typography::IconSize::Standard);
    const int itemHeight = qMax(::Spacing::ControlHeight::Small, itemContentHeight + 2 * m_itemVerticalPadding);
    for (QAction* action : actions()) {
        if (!action || !action->isVisible())
            continue;
        contentHeight += action->isSeparator() ? m_separatorHeight : itemHeight;
    }
    return QSize(qMax(base.width(), qMax(menu_tokens::minimumWidth, contentWidth) + 2 * m_shadowSize),
        qMax(base.height(), contentHeight));
}

void FluentMenu::showEvent(QShowEvent* event)
{
    QMenu::showEvent(event);

    if (property(kRestoreNativeMenuAnimationsProperty).toBool()) {
        setProperty(kRestoreNativeMenuAnimationsProperty, false);
        QApplication::setEffectEnabled(Qt::UI_AnimateMenu, true);
    }

    if (::fluent::overlay::syncInheritedThemeOverride(this, parentWidget()))
        onThemeUpdated();

    // On first macOS show, QMenu can enter showEvent() taller than sizeHint();
    // resize to sizeHint() instead of locking the transient excess via expandedTo().
    const QSize targetSize = sizeHint();
    if (size() != targetSize) {
        resize(targetSize);
        updateGeometry();
    }
    // QMenu's platform style can overwrite the mask inside base showEvent(); reapply ours afterward.
    updateSurfaceMask();

    if (m_cascadeParent) {
        positionCascade();
    } else {
        move(pos() - QPoint(m_shadowSize, m_shadowSize - themeSpacing().gap.tight));
    }
    normalizePopupLayering();
    QTimer::singleShot(0, this, [this]() {
        const QSize targetSize = sizeHint();
        bool geometryNeedsRefresh = false;
        for (QAction* action : actions()) {
            if (!action || action->isSeparator() || !action->isVisible())
                continue;
            if (actionGeometry(action).isEmpty()) {
                geometryNeedsRefresh = true;
                break;
            }
        }
        if (size() != targetSize)
            resize(targetSize);
        if (geometryNeedsRefresh) {
            // Invalidate QMenu's WASM pre-show one-row geometry cache with a 1 px nudge.
            resize(targetSize + QSize(0, 1));
            resize(targetSize);
        }
        updateSurfaceMask();
        positionCascade();
        normalizePopupLayering();
        update();
    });

    if (auto* previousAnimation = findChild<QVariantAnimation*>(QString::fromLatin1(kEntranceAnimationName),
            Qt::FindDirectChildrenOnly)) {
        previousAnimation->stop();
        delete previousAnimation;
    }

    m_revealProgress = m_translucentSurface ? 0.0 : 1.0;

    // Opaque runtimes cannot fade/clip through transparent pixels; show full menu immediately.
    if (!m_translucentSurface) {
        update();
        return;
    }

    // Drive height reveal and paint opacity together without changing native window opacity.
    auto* entranceAnimation = new QVariantAnimation(this);
    entranceAnimation->setObjectName(QString::fromLatin1(kEntranceAnimationName));
    entranceAnimation->setStartValue(0.0);
    entranceAnimation->setEndValue(1.0);
    entranceAnimation->setDuration(themeAnimation().fast);
    entranceAnimation->setEasingCurve(themeAnimation().decelerate);
    connect(entranceAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        const qreal progress = value.toReal();
        m_revealProgress = progress;
        update();
    });
    ::fluent::detail::startMotionTransition(entranceAnimation, themeAnimation().fast, true,
        QAbstractAnimation::DeleteWhenStopped);
}

void FluentMenu::normalizePopupLayering()
{
    if (!isVisible() || !compatibility::detail::popupWindowStackingSupported())
        return;

    QVector<FluentMenu*> menus;
    appendVisibleMenuChain(menus, rootMenuFor(this));
    for (FluentMenu* menu : std::as_const(menus))
        menu->raise();
}

void FluentMenu::positionCascade()
{
    QMenu* parent = m_cascadeParent.data();
    QAction* branch = parent ? parent->activeAction() : nullptr;
    if (!parent || !parent->isVisible() || !branch || branch->menu() != this)
        return;
    const QRect row = parent->actionGeometry(branch);
    if (row.isEmpty())
        return;

    int firstRowCenter = contentsMargins().top();
    for (QAction* action : actions()) {
        if (action->isVisible() && !action->isSeparator() && !actionGeometry(action).isEmpty()) {
            firstRowCenter = actionGeometry(action).center().y();
            break;
        }
    }
    QRect parentCard = parent->geometry();
    if (auto* fluentParent = qobject_cast<FluentMenu*>(parent)) {
        const int shadow = fluentParent->m_shadowSize;
        parentCard.adjust(shadow, shadow, -shadow, -shadow);
    }
    const QSize cardSize = size() - QSize(m_shadowSize * 2, m_shadowSize * 2);
    const int right = parentCard.right() + 1 + menu_tokens::cascadeGap;
    const int left = parentCard.left() - menu_tokens::cascadeGap - cardSize.width();
    const bool rtl = layoutDirection() == Qt::RightToLeft;
    int x = rtl ? left : right;
    int y = parent->mapToGlobal(row.center()).y() - firstRowCenter + m_shadowSize;
    if (QScreen* screen = parent->screen()) {
        const QRect available = screen->availableGeometry();
        if (x < available.left() || x + cardSize.width() > available.right() + 1)
            x = rtl ? right : left;
        x = qBound(available.left(), x, qMax(available.left(), available.right() + 1 - cardSize.width()));
        y = qBound(available.top(), y, qMax(available.top(), available.bottom() + 1 - cardSize.height()));
    }
    move(x - m_shadowSize, y - m_shadowSize);
}

} // namespace fluent::menus_toolbars

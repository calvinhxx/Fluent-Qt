#include "Menu.h"

#include <QActionEvent>
#include <QEvent>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QShowEvent>
#include <QPropertyAnimation>
#include <QTimer>
#include <QVariantAnimation>
#include <QEasingCurve>
#include <QFontMetrics>

#include "design/Typography.h"
#include "components/foundation/overlay/OverlayGeometry.h"
#include "components/foundation/overlay/OverlayShadow.h"
#include "components/foundation/private/SurfacePainter_p.h"
#include "compatibility/QtCompat.h"

namespace fluent::menus_toolbars {

namespace {
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
        result = qMax(result, fontMetrics.horizontalAdvance(menuLabelText(action->text())));
    }
    return result;
}

int maxShortcutWidth(const FluentMenu* menu, const QFontMetrics& fontMetrics)
{
    int result = 0;
    for (QAction* action : menu->actions()) {
        if (!action || !action->isVisible() || action->isSeparator())
            continue;
        result = qMax(result, fontMetrics.horizontalAdvance(menu->shortcutTextForAction(action)));
    }
    return result;
}

bool hasSubmenuAction(const FluentMenu* menu)
{
    for (QAction* action : menu->actions()) {
        if (action && action->isVisible() && !action->isSeparator() && action->menu())
            return true;
    }
    return false;
}

bool hasCheckableAction(const FluentMenu* menu)
{
    for (QAction* action : menu->actions()) {
        if (action && action->isVisible() && !action->isSeparator() && action->isCheckable())
            return true;
    }
    return false;
}

bool supportsPopupRaise()
{
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST"))
        return false;

    const QString platformName = QGuiApplication::platformName();
    return platformName != QStringLiteral("offscreen")
        && platformName != QStringLiteral("minimal");
}

bool hasMenuParent(const QWidget* widget)
{
    return widget && qobject_cast<const QMenu*>(widget->parentWidget());
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

// =============================== FluentMenuItem ===============================

FluentMenuItem::FluentMenuItem(const QString& text, QObject* parent)
    : QWidgetAction(parent) {
    setText(text);
    setFont(themeFont(m_fontStyle).toQFont());
}

void FluentMenuItem::setFontStyle(const QString& style) {
    if (m_fontStyle == style) return;
    m_fontStyle = style;
    onThemeUpdated();
    emit fontStyleChanged();
}

void FluentMenuItem::onThemeUpdated() {
    setFont(themeFont(m_fontStyle).toQFont());
}

// ================================ FluentMenu =================================

FluentMenu::FluentMenu(const QString& title, QWidget* parent)
    : QMenu(title, parent) {
    // Frameless top-level with system shadow disabled; shadow and rounding are
    // painted by the menu itself.
    // zh_CN: 顶层无边框并禁用系统阴影，阴影与圆角由自身绘制。
    setWindowFlags((windowFlags() & ~Qt::WindowType_Mask)
                   | Qt::Popup
                   | Qt::FramelessWindowHint
                   | Qt::NoDropShadowWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_Hover);
    setAutoFillBackground(false);
    setContentsMargins(m_shadowSize, m_shadowSize, m_shadowSize, m_shadowSize);

    setFont(themeFont(m_fontStyle).toQFont());
    onThemeUpdated();
}

void FluentMenu::setFontStyle(const QString& style) {
    if (m_fontStyle == style) return;
    m_fontStyle = style;
    onThemeUpdated();
    emit fontStyleChanged();
}

void FluentMenu::onThemeUpdated() {
    const auto& s = themeSpacing();
    int vPadding = s.gap.tight; // 4px

    // Sync the menu font; it feeds QMenu's internal actionGeometry heights.
    // zh_CN: 同步菜单字体（影响 QMenu 内部 actionGeometry 高度计算）。
    setFont(themeFont(m_fontStyle).toQFont());

    // Reserve shadow and inner padding via margins so QMenu's sizeHint includes
    // them and the popup window is large enough.
    // zh_CN: 用 margins 为阴影和内部 padding 预留空间，QMenu 的 sizeHint 会自动
    // 包含这些边距，确保窗口足够大。
    setContentsMargins(m_shadowSize, m_shadowSize + vPadding, m_shadowSize, m_shadowSize + vPadding);
    const int separatorHeight = s.gap.normal + 1;
    const int itemVerticalPadding = s.padding.listItemV;
    setStyleSheet(QStringLiteral(
        "QMenu { background-color: transparent; border: 0px; padding: 0px; }"
        "QMenu::item { background-color: transparent; padding: %1px 0px; margin: 0px; }"
        "QMenu::separator { height: %2px; }"
    ).arg(itemVerticalPadding).arg(separatorHeight));
    setMinimumWidth(sizeHint().width());
    updateGeometry();
    update();
}

void FluentMenu::actionEvent(QActionEvent* event)
{
    QMenu::actionEvent(event);
    if (event->type() == QEvent::ActionAdded && event->action() && !event->action()->isSeparator()) {
        QAction* action = event->action();
        QObject::connect(action, &QAction::triggered, this, [this, action]() {
            if (!action || action->isSeparator() || action->menu())
                return;
            for (QObject* obj = this; obj; obj = obj->parent()) {
                if (auto* menu = qobject_cast<QMenu*>(obj))
                    menu->hide();
            }
        });
    }

    if (event->type() == QEvent::ActionAdded || event->type() == QEvent::ActionRemoved || event->type() == QEvent::ActionChanged) {
        setMinimumWidth(sizeHint().width());
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

    QMenu::keyPressEvent(event);
}

void FluentMenu::mousePressEvent(QMouseEvent* event)
{
    QMenu::mousePressEvent(event);
    normalizePopupLayering();
    QTimer::singleShot(0, this, [this]() {
        normalizePopupLayering();
    });
}

void FluentMenu::mouseReleaseEvent(QMouseEvent* event)
{
    QMenu::mouseReleaseEvent(event);
    normalizePopupLayering();
    QTimer::singleShot(0, this, [this]() {
        normalizePopupLayering();
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

    const int trailingColumn = hasSubmenuAction(this) ? ::Spacing::ControlHeight::Small : ::Spacing::Small;
    QRect rect = actionGeometry(action);
    rect.setLeft(m_shadowSize);
    rect.setWidth(width() - 2 * m_shadowSize);
    const int right = rect.right() - ::Spacing::Padding::ControlHorizontal - trailingColumn;
    return QRect(qMax(rect.left(), right - shortcutColumn + 1), rect.top(), shortcutColumn, rect.height());
}

QRect FluentMenu::itemSubmenuIndicatorGeometry(QAction* action) const
{
    if (!action || !action->menu() || actionGeometry(action).isEmpty())
        return QRect();

    QRect rect = actionGeometry(action);
    rect.setLeft(m_shadowSize);
    rect.setWidth(width() - 2 * m_shadowSize);
    const int side = ::Spacing::ControlHeight::Small;
    return QRect(rect.right() - ::Spacing::Padding::ControlHorizontal - side + 1,
                 rect.top() + (rect.height() - side) / 2,
                 side,
                 side);
}

void FluentMenu::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    // 1. Clear to transparent. zh_CN: 透明清屏。
    p.setCompositionMode(QPainter::CompositionMode_Source);
    p.fillRect(rect(), Qt::transparent);
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);

    const auto& colors = themeColors();
    const auto& spacing = themeSpacing();
    const auto& radius = themeRadius();

    // Design language drives the surface stroke and per-item highlight treatment below; resolve it
    // once up front. Fluent (default) is unchanged. zh_CN: 设计语言决定下方的表面描边与逐项高亮处理;在此
    // 一次性解析。Fluent(默认)保持不变。
    const DesignLanguage lang = themeDesignLanguage();
    // Theme-aware interaction veil: a translucent overlay that DARKENS light surfaces and LIGHTENS
    // dark ones, so a neutral on-surface state layer stays visible under both App themes. zh_CN: 主题
    // 感知交互薄层:浅色面变暗、深色面变亮,使中性 on-surface state layer 在明暗两主题下都可见。
    const bool dark = effectiveTheme() == Dark;
    const auto veil = [dark](int a) { return dark ? QColor(255, 255, 255, a) : QColor(0, 0, 0, a); };

    // 2. Vertical extent of the items. zh_CN: 计算 items 垂直范围。
    QRect itemsRect;
    for (QAction* action : actions()) {
        if (action->isVisible()) {
            itemsRect |= actionGeometry(action);
        }
    }

    if (itemsRect.isEmpty()) return;

    // The card rect depends only on the final popup size, not on QMenu's
    // internal action widths.
    // zh_CN: 底板矩形只依赖最终 popup 尺寸，不依赖 QMenu 内部 action 宽度。
    QRect contentRect = rect().adjusted(m_shadowSize, m_shadowSize, -m_shadowSize, -m_shadowSize);

    // Reveal animation: clip downward from the top while progress < 1.
    // zh_CN: 揭示动画：progress < 1 时从顶部向下裁剪可见高度。
    if (m_revealProgress < 1.0)
        p.setClipRect(QRectF(0, 0, width(), height() * m_revealProgress));

    // 3. Paint the layered soft shadow. zh_CN: 绘制多层软阴影。
    drawShadow(p, contentRect);

    // 4. Paint the rounded background and border. zh_CN: 绘制圆角背景与边框。
    int r = radius.overlay;
    p.save();
    QPainterPath clipPath;
    clipPath.addRoundedRect(contentRect, r, r);
    p.setClipPath(clipPath);

    // Card stroke: Fluent + macOS keep a hairline border (Fluent → strokeCard, macOS → a hairline
    // strokeDefault); Material 3's surface-container panel drops the visible border and leans on the
    // existing layered shadow for elevation. The bgLayer fill + overlay radius are shared. zh_CN: 底板
    // 描边:Fluent + macOS 保留发丝边框(Fluent→strokeCard,macOS→发丝 strokeDefault);Material 3 的
    // surface-container 面板去掉可见边框,改由既有多层阴影表达高程。bgLayer 填充 + overlay 圆角共用。
    fluent::painting::RoundedSurfacePaint surface;
    surface.fill = colors.bgLayer;
    surface.radius = r;
    if (lang == DesignMaterial)
        surface.border = Qt::transparent;
    else if (lang == DesignCupertino)
        surface.border = colors.strokeDefault;
    else
        surface.border = colors.strokeCard;
    fluent::painting::paintRoundedSurface(p, QRectF(contentRect), surface);

    // 5. Paint the menu items.
    // itemInset: shared horizontal inset for highlights and separators
    // (4px, visually aligned with the card rounding).
    // textPadding: horizontal text padding from the card edge (12px, ControlHorizontal).
    // zh_CN: 绘制菜单项。itemInset：高亮背景与分割线距底板边缘的统一水平缩进
    // （4px，与底板圆角视觉对齐）；textPadding：文字距底板边缘的水平内边距
    // （12px，ControlHorizontal）。
    const int plateLeft    = contentRect.left();
    const int plateWidth   = contentRect.width();
    const int itemInset    = spacing.gap.tight;          // 4
    const int textPadding  = spacing.padding.controlH;   // 12
    const int checkColumn  = hasCheckableAction(this) ? spacing.controlHeight.small : 0; // 24 only when needed
    const QFontMetrics fontMetrics(font());
    const int shortcutColumn = maxShortcutWidth(this, fontMetrics);
    const int trailingColumn = hasSubmenuAction(this) ? ::Spacing::ControlHeight::Small : ::Spacing::Small;

    // Set the paint font explicitly so the shadow pass cannot pollute it.
    // zh_CN: 明确设置绘制字体，防止 shadow 循环中字体被污染。
    p.setFont(font());

    for (QAction* action : actions()) {
        if (!action->isVisible()) continue;

        QRect itemRect = actionGeometry(action);
        // Normalize the horizontal span to the card bounds (actionGeometry may
        // exclude the shadow margin).
        // zh_CN: 规范化水平范围：统一对齐到底板边界（actionGeometry 可能不含 shadow margin）。
        itemRect.setLeft(plateLeft);
        itemRect.setWidth(plateWidth);

        if (!contentRect.intersects(itemRect)) continue;

        if (action->isSeparator()) {
            p.setPen(colors.strokeDivider);
            int y = itemRect.center().y();
            p.drawLine(itemRect.left() + itemInset, y, itemRect.right() - itemInset, y);
            continue;
        }

        bool isEnabled = action->isEnabled();
        bool isActive  = (action == activeAction());
        const bool highlighted = isEnabled && (action->isChecked() || isActive);

        // macOS highlights the active item with a SOLID accent selection bar and flips all of its
        // text/glyphs to white; so flag it here to override primaryText/secondaryText below. zh_CN:
        // macOS 用实心 accent 选择条高亮当前项,并将其文字/字形全部翻白;在此置位,供下方覆盖
        // primaryText/secondaryText。
        const bool cupertinoActiveBar = (lang == DesignCupertino) && highlighted;

        // Item highlight fill. zh_CN: 逐项高亮填充。
        //  - Fluent (default): solid subtleSecondary rounded rect. zh_CN: 实心 subtleSecondary 圆角块。
        //  - Material 3: a NEUTRAL on-surface state layer (veil ~8%/0x14), a translucent veil over the
        //    item rather than a fill swap, inscribed in the rounded rect (§4). zh_CN: 中性 on-surface
        //    state layer(veil ~8%/0x14),为半透明薄层而非换填充,内切于圆角矩形(§4)。
        //  - macOS: the classic SOLID accentDefault selection bar. zh_CN: 经典实心 accentDefault 选择条。
        QColor bg = Qt::transparent;
        if (highlighted) {
            if (lang == DesignMaterial)
                bg = veil(0x14);              // ~8% neutral on-surface state layer
            else if (lang == DesignCupertino)
                bg = colors.accentDefault;    // solid blue selection bar
            else
                bg = colors.subtleSecondary;  // Fluent (unchanged)
        }

        // §2 invalid-QColor guard: only paint a valid, non-transparent fill. zh_CN: §2 无效 QColor 防护:
        // 仅在色值有效且非透明时绘制填充。
        if (bg.isValid() && bg.alpha() > 0) {
            QRectF bgRect = itemRect.adjusted(itemInset, 1, -itemInset, -1);
            p.setPen(Qt::NoPen);
            p.setBrush(bg);
            p.drawRoundedRect(bgRect, radius.control, radius.control);
        }

        // For the macOS active bar, label + shortcut + checkmark + chevron all read white on accent;
        // override BOTH text roles. zh_CN: macOS 活动条上,标签 + 快捷键 + 勾选 + 箭头都在 accent 上读作白色;
        // 故同时覆盖两个文字角色。
        const QColor primaryText = cupertinoActiveBar ? colors.textOnAccent
                                                      : (isEnabled ? colors.textPrimary : colors.textDisabled);
        const QColor secondaryText = cupertinoActiveBar ? colors.textOnAccent
                                                        : (isEnabled ? colors.textSecondary : colors.textDisabled);

        if (action->isCheckable() && action->isChecked()) {
            QRect checkRect(itemRect.left() + itemInset,
                            itemRect.top() + (itemRect.height() - checkColumn) / 2,
                            checkColumn,
                            checkColumn);
            p.setPen(primaryText);
            Typography::Icons::paintGlyph(
                p, QRectF(checkRect), Typography::Icons::CheckMark,
                Typography::IconSize::Compact, Qt::AlignCenter);
        }

        const QString shortcutText = shortcutTextForAction(action);
        const QRect shortcutRect = itemShortcutGeometry(action);
        const int shortcutReserve = shortcutColumn > 0 ? shortcutColumn + spacing.gap.section : 0;
        const int textRight = itemRect.right() - textPadding - trailingColumn - shortcutReserve;
        QRect textRect(itemRect.left() + textPadding + checkColumn,
                       itemRect.top(),
                       qMax(0, textRight - (itemRect.left() + textPadding + checkColumn) + 1),
                       itemRect.height());

        p.setPen(primaryText);
        p.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine, menuLabelText(action->text()));

        if (!shortcutRect.isEmpty()) {
            p.setPen(secondaryText);
            p.drawText(shortcutRect, Qt::AlignVCenter | Qt::AlignRight | Qt::TextSingleLine, shortcutText);
        }

        if (action->menu()) {
            const QRect arrowRect = itemSubmenuIndicatorGeometry(action);
            p.setPen(secondaryText);
            Typography::Icons::paintGlyph(
                p, QRectF(arrowRect), Typography::Icons::ChevronRightMed,
                Typography::IconSize::Compact, Qt::AlignCenter);
        }
    }
    p.restore();
}

QSize FluentMenu::sizeHint() const
{
    QSize base = QMenu::sizeHint();
    const QFontMetrics fontMetrics(font());

    const int shortcutTextWidth = maxShortcutWidth(this, fontMetrics);
    const int shortcutColumn = shortcutTextWidth > 0 ? shortcutTextWidth + ::Spacing::Gap::Section : 0;
    const int trailingColumn = hasSubmenuAction(this) ? ::Spacing::ControlHeight::Small : ::Spacing::Small;
    const int checkColumn = hasCheckableAction(this) ? ::Spacing::ControlHeight::Small : 0;
    const int contentWidth = ::Spacing::Gap::Tight * 2
                           + ::Spacing::Padding::ControlHorizontal * 2
                           + checkColumn
                           + maxLabelWidth(this, fontMetrics)
                           + shortcutColumn
                           + trailingColumn;
    return QSize(qMax(base.width(), contentWidth + 2 * m_shadowSize), base.height());
}

void FluentMenu::showEvent(QShowEvent* event) {
    QMenu::showEvent(event);

    if (::fluent::overlay::syncInheritedThemeOverride(this, parentWidget()))
        onThemeUpdated();

    const QSize preferredSize = sizeHint();
    const QSize targetSize = size().expandedTo(preferredSize);
    if (size() != targetSize) {
        resize(targetSize);
        updateGeometry();
    }

    // Compensate the shadow margin: top-level menus cancel it fully, cascaded
    // submenus keep a small gap so the parent's shadow never covers their content.
    // zh_CN: 修正阴影 margin 偏移——顶层菜单完整抵消阴影；级联子菜单保留小间距，
    // 避免父菜单阴影压到子菜单内容。
    const auto& spacing = themeSpacing();
    QPoint targetPos = pos();
    const int horizontalOffset = hasMenuParent(this)
        ? m_shadowSize - spacing.gap.tight
        : m_shadowSize;
    targetPos.rx() -= horizontalOffset;
    targetPos.ry() -= (m_shadowSize - spacing.gap.tight);
    move(targetPos);
    normalizePopupLayering();
    QTimer::singleShot(0, this, [this]() {
        normalizePopupLayering();
    });

    // Initial state: fully transparent with reveal progress at zero.
    // zh_CN: 初始状态：完全透明，揭示进度归零。
    m_revealProgress = 0.0;
    setWindowOpacity(0.0);

    // Height reveal animation, expanding downward to match the WinUI 3
    // PopupThemeTransition.
    // zh_CN: 高度揭示动画（从顶部向下展开，匹配 WinUI 3 PopupThemeTransition）。
    auto* revealAnim = new QVariantAnimation(this);
    revealAnim->setStartValue(0.0);
    revealAnim->setEndValue(1.0);
    revealAnim->setDuration(themeAnimation().fast);
    revealAnim->setEasingCurve(themeAnimation().decelerate);
    connect(revealAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
        m_revealProgress = v.toReal();
        update();
    });
    revealAnim->start(QAbstractAnimation::DeleteWhenStopped);

    // Opacity fade-in animation. zh_CN: 透明度淡入动画。
    auto* opacityAnim = new QPropertyAnimation(this, "windowOpacity");
    opacityAnim->setDuration(themeAnimation().fast);
    opacityAnim->setStartValue(0.0);
    opacityAnim->setEndValue(1.0);
    opacityAnim->setEasingCurve(themeAnimation().decelerate);
    opacityAnim->start(QAbstractAnimation::DeleteWhenStopped);
}

void FluentMenu::normalizePopupLayering()
{
    if (!isVisible() || !supportsPopupRaise())
        return;

    QVector<FluentMenu*> menus;
    appendVisibleMenuChain(menus, rootMenuFor(this));
    for (FluentMenu* menu : std::as_const(menus))
        menu->raise();
}

void FluentMenu::drawShadow(QPainter& painter, const QRect& contentRect) {
    // Menus float close to their anchor, so they carry a lighter shadow than
    // modal surfaces.
    // zh_CN: 菜单紧贴锚点浮动，阴影强度低于模态表面。
    ::fluent::overlay::paintLayeredShadow(painter, contentRect, themeRadius().overlay,
                                          themeShadow(Elevation::High),
                                          /*intensity=*/0.18);
}

} // namespace fluent::menus_toolbars

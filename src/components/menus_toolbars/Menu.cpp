#include "Menu.h"

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
#include <QShowEvent>
#include <QTimer>
#include <QVariantAnimation>
#include "compatibility/QtCompat.h"
#include "compatibility/private/RuntimePlatformCapabilities_p.h"
#include "components/foundation/overlay/OverlayGeometry.h"
#include "components/foundation/overlay/OverlayShadow.h"
#include "components/foundation/private/SurfacePainter_p.h"
#include "design/Typography.h"

namespace fluent::menus_toolbars {

namespace {
constexpr char kEntranceAnimationName[] =
    "fluentMenuEntranceAnimation";
constexpr char kRestoreNativeMenuAnimationsProperty[] =
    "_fluent_restoreNativeMenuAnimations";

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
        result = qMax(result, qMax(fontMetrics.horizontalAdvance(text),
                                  fontMetrics.boundingRect(text).width()));
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
        result = qMax(result, qMax(fontMetrics.horizontalAdvance(text),
                                  fontMetrics.boundingRect(text).width()));
    }
    // Leave ink allowance on both sides: Windows font backends can render
    // antialiased bearings beyond both horizontalAdvance() and boundingRect().
    // zh_CN: 两侧均预留字形墨迹空间；Windows 字体后端的抗锯齿边缘可能超出
    // horizontalAdvance() 与 boundingRect()，不能用恰好等宽的矩形绘制。
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

void FluentMenuItem::setFontStyle(Typography::FontRole role) {
    if (m_fontStyle == role) return;
    m_fontStyle = role;
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
    m_translucentSurface =
        compatibility::detail::runtimePlatformCapabilities()
            .translucentPopupSurfaces;
    m_shadowSize = m_translucentSurface ? ::Spacing::Standard : 0;
    setAttribute(Qt::WA_TranslucentBackground, m_translucentSurface);
    setAttribute(Qt::WA_OpaquePaintEvent, !m_translucentSurface);
    setAttribute(Qt::WA_Hover);
    setAutoFillBackground(false);
    setContentsMargins(m_shadowSize, m_shadowSize, m_shadowSize, m_shadowSize);

    connect(this, &QMenu::aboutToShow, this, [this]() {
        if (!QApplication::isEffectEnabled(Qt::UI_AnimateMenu))
            return;

        // QMenu selects its process-wide platform animation after emitting
        // aboutToShow(). Suppress that selection only until showEvent() starts;
        // FluentMenu supplies its own paint-only entrance transition below.
        // zh_CN: QMenu 会在发出 aboutToShow() 后选择进程级平台动画；仅在
        // showEvent() 开始前暂时抑制该选择，FluentMenu 在下方提供自绘入场动画。
        setProperty(kRestoreNativeMenuAnimationsProperty, true);
        QApplication::setEffectEnabled(Qt::UI_AnimateMenu, false);

        // Empty menus can return before showEvent(), and aboutToShow handlers
        // may delete the menu. Always retain a queued restoration fallback.
        // zh_CN: 空菜单可能在 showEvent() 前返回，aboutToShow 处理器也可能
        // 删除菜单，因此始终保留一次队列恢复兜底。
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

void FluentMenu::setFontStyle(Typography::FontRole role) {
    if (m_fontStyle == role) return;
    m_fontStyle = role;
    onThemeUpdated();
    emit fontStyleChanged();
}

void FluentMenu::onThemeUpdated() {
    const auto& s = themeSpacing();
    const int vPadding = s.gap.tight;

    // Sync the menu font; it feeds QMenu's internal actionGeometry heights.
    // zh_CN: 同步菜单字体（影响 QMenu 内部 actionGeometry 高度计算）。
    setFont(themeFont(m_fontStyle).toQFont());

    // Reserve shadow and inner padding via margins so QMenu's sizeHint includes
    // them and the popup window is large enough.
    // zh_CN: 用 margins 为阴影和内部 padding 预留空间，QMenu 的 sizeHint 会自动
    // 包含这些边距，确保窗口足够大。
    setContentsMargins(m_shadowSize, m_shadowSize + vPadding, m_shadowSize, m_shadowSize + vPadding);
    setItemLayoutMetrics(s.padding.listItemV, s.gap.normal + 1);
    setMinimumWidth(sizeHint().width());
    updateSurfaceMask();
    updateGeometry();
    update();
}

void FluentMenu::setItemLayoutMetrics(int verticalPadding,
                                      int separatorHeight)
{
    m_itemVerticalPadding = qMax(0, verticalPadding);
    m_separatorHeight = qMax(1, separatorHeight);
    setStyleSheet(QStringLiteral(
        "QMenu { background-color: transparent; border: 0px; padding: 0px; }"
          "QMenu::item { background-color: transparent; padding: %1px 0px; "
          "margin: 0px; }"
        "QMenu::separator { height: %2px; }"
    ).arg(m_itemVerticalPadding).arg(m_separatorHeight));
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
    const int trailingColumn = hasSubmenuAction(this)
        ? spacing.controlHeight.small
        : ::Spacing::Small;
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
    return QRect(rect.right() - textPadding - side + 1,
                 rect.top() + (rect.height() - side) / 2,
                 side,
                 side);
}

void FluentMenu::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    const auto& colors = themeColorsRef();

    // 1. Clear alpha-capable popups; runtimes without popup alpha receive a
    // fully opaque Fluent layer so text never floats on an unpainted window.
    // zh_CN: 支持 alpha 时透明清屏；不支持 popup alpha 的运行时完整填充 Fluent
    // layer，避免文字漂浮在未绘制窗口上。
    p.setCompositionMode(QPainter::CompositionMode_Source);
    p.fillRect(rect(), m_translucentSurface ? Qt::transparent : colors.bgLayer);
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    // Fade only this custom paint pass. Applying QGraphicsOpacityEffect to a
    // native QMenu popup can leave a Wayland QMenu::exec() surface invisible
    // while its modal event loop continues to block user input.
    // zh_CN: 仅对当前自绘过程应用透明度；在原生 QMenu popup 上安装
    // QGraphicsOpacityEffect 可能使 Wayland 的 QMenu::exec() 表面不可见，
    // 但模态事件循环仍继续阻塞输入。
    p.setOpacity(m_translucentSurface
        ? qBound<qreal>(0.0, m_revealProgress, 1.0)
        : 1.0);

    const auto& spacing = themeSpacing();
    const auto& radius = themeRadius();

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
    if (m_translucentSurface && m_revealProgress < 1.0)
        p.setClipRect(QRectF(0, 0, width(), height() * m_revealProgress));

    // 3. Paint the layered soft shadow. zh_CN: 绘制多层软阴影。
    if (m_translucentSurface)
        drawShadow(p, contentRect);

    // 4. Paint the rounded background and border. zh_CN: 绘制圆角背景与边框。
    int r = radius.overlay;
    p.save();
    QPainterPath clipPath;
    clipPath.addRoundedRect(contentRect, r, r);
    p.setClipPath(clipPath);
    fluent::painting::RoundedSurfacePaint surface;
    surface.fill = colors.bgLayer;
    surface.radius = r;
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
    const int textPadding  = spacing.padding.controlH;
    const int leadingColumn = hasLeadingAction(this)
        ? spacing.controlHeight.small
        : 0;
    const QFontMetrics fontMetrics(font());
    const int shortcutColumn = maxShortcutWidth(this, fontMetrics);
    const int trailingColumn = hasSubmenuAction(this)
        ? spacing.controlHeight.small
        : ::Spacing::Small;
    const int shortcutGap = spacing.gap.section;

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
        const QColor bg = highlighted
            ? colors.subtleSecondary
            : QColor(Qt::transparent);

        // §2 invalid-QColor guard: only paint a valid, non-transparent fill. zh_CN: §2 无效 QColor 防护:
        // 仅在色值有效且非透明时绘制填充。
        if (bg.isValid() && bg.alpha() > 0) {
            QRectF bgRect = itemRect.adjusted(itemInset, 1, -itemInset, -1);
            p.setPen(Qt::NoPen);
            p.setBrush(bg);
            p.drawRoundedRect(bgRect, radius.control, radius.control);
        }
        const QColor primaryText = isEnabled ? colors.textPrimary : colors.textDisabled;
        const QColor secondaryText = isEnabled ? colors.textSecondary : colors.textDisabled;

        if (leadingColumn > 0) {
            const QRect leadingRect(itemRect.left() + itemInset,
                                    itemRect.top() + (itemRect.height() - leadingColumn) / 2,
                                    leadingColumn,
                                    leadingColumn);
            if (action->isCheckable() && action->isChecked()) {
                p.setPen(primaryText);
                Typography::Icons::paintGlyph(
                    p, QRectF(leadingRect), Typography::Icons::CheckMark,
                    Typography::IconSize::Compact, Qt::AlignCenter);
            } else if (!action->icon().isNull()) {
                const QIcon::Mode mode = !isEnabled ? QIcon::Disabled
                                                    : (isActive ? QIcon::Active : QIcon::Normal);
                const QIcon::State state = action->isChecked() ? QIcon::On : QIcon::Off;
                // The leading column is 24 px for alignment, but WinUI menu
                // command icons use a 16 px optical slot. Painting into the
                // whole column lets QIcon upscale a 16 px source to 24 px,
                // making editing glyphs visually dominate their labels.
                const int iconSide = qMin(
                    Typography::IconSize::Standard,
                    qMin(leadingRect.width(), leadingRect.height()));
                const QRect iconRect(
                    leadingRect.center().x() - iconSide / 2,
                    leadingRect.center().y() - iconSide / 2,
                    iconSide,
                    iconSide);
                action->icon().paint(
                    &p, iconRect, Qt::AlignCenter, mode, state);
            }
        }

        const QString shortcutText = shortcutTextForAction(action);
        const QRect shortcutRect = itemShortcutGeometry(action);
        const int shortcutReserve = shortcutColumn > 0 ? shortcutColumn + shortcutGap : 0;
        const int textRight = itemRect.right() - textPadding - trailingColumn - shortcutReserve;
        const int textLeft = itemRect.left() + textPadding + leadingColumn;
        const int textBaseline = itemRect.top()
            + (itemRect.height() - fontMetrics.height()) / 2
            + fontMetrics.ascent();

        p.setPen(primaryText);
        // Draw from a baseline instead of an exactly measured QRect. Some
        // Windows font backends paint antialiased edge pixels outside Qt's
        // reported text bounds; QRect-based drawText clips those pixels and
        // can visibly remove the final "l" or the leading "C".
        // zh_CN: 使用基线坐标绘制，不再使用恰好等宽的 QRect。部分 Windows
        // 字体后端会将抗锯齿边缘绘制到 Qt 报告边界之外，矩形绘制会裁掉末尾
        // “l” 或开头 “C”。
        if (textLeft <= textRight)
            p.drawText(QPoint(textLeft, textBaseline), menuLabelText(action->text()));

        if (!shortcutRect.isEmpty()) {
            p.setPen(secondaryText);
            constexpr int shortcutInkInset = 6;
            const int shortcutX = shortcutRect.right()
                - shortcutInkInset
                - fontMetrics.horizontalAdvance(shortcutText) + 1;
            p.drawText(QPoint(shortcutX, textBaseline), shortcutText);
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

void FluentMenu::resizeEvent(QResizeEvent* event) {
    QMenu::resizeEvent(event);
    updateSurfaceMask();
}

void FluentMenu::updateSurfaceMask() {
    if (m_translucentSurface || rect().isEmpty()) {
        // Do not clear a native/platform mask that QMenu may own. Only remove
        // the opaque fallback mask installed by this class.
        // zh_CN: 不清除 QMenu 可能持有的平台原生 mask；只移除本类为不透明
        // 回退表面安装的遮罩。
        if (m_surfaceMaskApplied) {
            clearMask();
            m_surfaceMaskApplied = false;
        }
        return;
    }

    // Browser popup windows use an opaque backing surface for stable text and
    // animation. Clip that surface at the widget/window boundary so the full-
    // rect opaque clear cannot leak through the painted rounded corners.
    // QRegion is intentionally used only for the opaque fallback; translucent
    // desktop popups keep their antialiased alpha edge and painted shadow.
    // zh_CN: 浏览器 popup 使用不透明后备表面以保证文字与动画稳定。通过控件/
    // 窗口边界遮罩裁掉矩形清屏的四角，避免覆盖后续绘制的圆角轮廓。QRegion 只
    // 用于不透明回退；桌面透明 popup 继续使用抗锯齿 alpha 边缘与自绘阴影。
    const QRegion surfaceMask = ::fluent::overlay::roundedRectRegion(
        rect(), themeRadius().overlay);
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
    const int trailingColumn = hasSubmenuAction(this)
        ? spacing.controlHeight.small
        : ::Spacing::Small;
    const int leadingColumn = hasLeadingAction(this)
        ? spacing.controlHeight.small
        : 0;
    const int textPadding = spacing.padding.controlH;
    const int contentWidth = spacing.gap.tight * 2
                           + textPadding * 2
                           + leadingColumn
                           + maxLabelWidth(this, fontMetrics)
                           + shortcutColumn
                           + trailingColumn;

    // Qt WASM can report only one row from QMenu::sizeHint() for a popup with
    // several actions. Compute a backend-independent lower bound from the
    // visible action list; the later resize lets QMenu rebuild actionGeometry
    // for every row while native platforms retain their larger style hint.
    // zh_CN: Qt WASM 对多动作 popup 的 QMenu::sizeHint() 有时只返回一行高度。
    // 根据可见动作列表计算与后端无关的下界；随后 resize 会让 QMenu 为每一行重建
    // actionGeometry，原生平台仍保留其更大的 style hint。
    int contentHeight = contentsMargins().top() + contentsMargins().bottom();
    const int itemContentHeight = qMax(fontMetrics.height(),
                                       Typography::IconSize::Standard);
    const int itemHeight = qMax(::Spacing::ControlHeight::Small,
                                itemContentHeight + 2 * m_itemVerticalPadding);
    for (QAction* action : actions()) {
        if (!action || !action->isVisible())
            continue;
        contentHeight += action->isSeparator()
            ? m_separatorHeight
            : itemHeight;
    }
    return QSize(qMax(base.width(), contentWidth + 2 * m_shadowSize),
                 qMax(base.height(), contentHeight));
}

void FluentMenu::showEvent(QShowEvent* event) {
    QMenu::showEvent(event);

    if (property(kRestoreNativeMenuAnimationsProperty).toBool()) {
        setProperty(kRestoreNativeMenuAnimationsProperty, false);
        QApplication::setEffectEnabled(Qt::UI_AnimateMenu, true);
    }

    if (::fluent::overlay::syncInheritedThemeOverride(this, parentWidget()))
        onThemeUpdated();

    // QMenu can enter showEvent() with a transient platform-window height that
    // is larger than its already-settled sizeHint (notably on the first macOS
    // popup). Treat the Fluent/native-combined hint as authoritative instead
    // of preserving that one-shot excess with expandedTo(). Fixed widget size
    // constraints are still honored by QWidget::resize().
    // zh_CN: QMenu 首次进入 showEvent() 时，平台窗口高度可能暂时大于已经稳定的
    // sizeHint（macOS 尤其明显）。以 Fluent/native 合并后的 hint 为准，避免
    // expandedTo() 锁住这次性的多余高度；QWidget::resize() 仍会遵守固定尺寸约束。
    const QSize targetSize = sizeHint();
    if (size() != targetSize) {
        resize(targetSize);
        updateGeometry();
    }
    // QMenu's platform style may replace a mask while processing its base
    // showEvent (macOS keeps only its own native corner convention). Reapply
    // the Fluent surface mask after base-show geometry has settled.
    // zh_CN: QMenu 平台样式可能在基类 showEvent 中替换 mask（macOS 会保留
    // 自己的原生圆角约定）；基类显示几何稳定后重新应用 Fluent 表面遮罩。
    updateSurfaceMask();

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
            // QMenu's WASM backend can retain its pre-popup one-row cache when
            // resized from inside showEvent(). A one-pixel post-show nudge
            // invalidates that cache, then restores the token-derived size.
            // zh_CN: QMenu 的 WASM 后端可能保留 showEvent() 之前的单行缓存。
            // 显示后一像素的尺寸扰动会使其失效，再恢复 token 计算出的尺寸。
            resize(targetSize + QSize(0, 1));
            resize(targetSize);
        }
        updateSurfaceMask();
        normalizePopupLayering();
        update();
    });

    if (auto* previousAnimation =
            findChild<QVariantAnimation*>(
                QString::fromLatin1(kEntranceAnimationName),
                Qt::FindDirectChildrenOnly)) {
        previousAnimation->stop();
        delete previousAnimation;
    }

    // Initial state: custom paint is transparent with reveal progress at zero.
    // Native window opacity and graphics effects deliberately remain untouched.
    // zh_CN: 初始状态：自绘内容透明且揭示进度归零；不修改原生窗口透明度，
    // 也不在顶层菜单安装 graphics effect。
    m_revealProgress = m_translucentSurface ? 0.0 : 1.0;

    // Opaque popup runtimes cannot reveal through transparent pixels. Paint
    // the complete stable card immediately instead of flashing a partial menu.
    // zh_CN: 不透明 popup 运行时无法通过透明像素做揭示动画，直接绘制完整稳定
    // 菜单，避免闪现残缺表面。
    if (!m_translucentSurface) {
        update();
        return;
    }

    // One timeline drives both the height reveal and paint opacity, keeping
    // the WinUI PopupThemeTransition without native-window opacity calls.
    // zh_CN: 用同一时间线驱动高度揭示与自绘透明度，在保留 WinUI
    // PopupThemeTransition 的同时避开原生窗口透明度接口。
    auto* entranceAnimation = new QVariantAnimation(this);
    entranceAnimation->setObjectName(
        QString::fromLatin1(kEntranceAnimationName));
    entranceAnimation->setStartValue(0.0);
    entranceAnimation->setEndValue(1.0);
    entranceAnimation->setDuration(themeAnimation().fast);
    entranceAnimation->setEasingCurve(themeAnimation().decelerate);
    connect(
        entranceAnimation,
        &QVariantAnimation::valueChanged,
        this,
        [this](const QVariant& value) {
            const qreal progress = value.toReal();
            m_revealProgress = progress;
            update();
        });
    entranceAnimation->start(
        QAbstractAnimation::DeleteWhenStopped);
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

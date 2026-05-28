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

namespace view::menus_toolbars {

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
    // 顶层无边框 + 禁用系统阴影，由自身绘制阴影与圆角
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

    // 同步菜单字体（影响 QMenu 内部的 actionGeometry 高度计算）
    setFont(themeFont(m_fontStyle).toQFont());

    // 使用 margins 为阴影和内部 padding 预留空间
    // 这样 QMenu 的 sizeHint 会自动包含这些边距，确保窗口足够大
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
        const QKeySequence pressed(event->keyCombination());
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

    // 1. 透明清屏
    p.setCompositionMode(QPainter::CompositionMode_Source);
    p.fillRect(rect(), Qt::transparent);
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);

    const auto& colors = themeColors();
    const auto& spacing = themeSpacing();
    const auto& radius = themeRadius();

    // 2. 计算 items 垂直范围
    QRect itemsRect;
    for (QAction* action : actions()) {
        if (action->isVisible()) {
            itemsRect |= actionGeometry(action);
        }
    }

    if (itemsRect.isEmpty()) return;

    // 底板矩形只依赖最终 popup 尺寸，不依赖 QMenu 内部 action 宽度。
    QRect contentRect = rect().adjusted(m_shadowSize, m_shadowSize, -m_shadowSize, -m_shadowSize);

    // 揭示动画：从顶部向下裁剪（progress < 1 时限制可见高度）
    if (m_revealProgress < 1.0)
        p.setClipRect(QRectF(0, 0, width(), height() * m_revealProgress));

    // 3. 绘制多层软阴影
    drawShadow(p, contentRect);

    // 4. 绘制圆角背景与边框
    int r = radius.overlay;
    p.save();
    QPainterPath clipPath;
    clipPath.addRoundedRect(contentRect, r, r);
    p.setClipPath(clipPath);

    p.setPen(colors.strokeCard);
    p.setBrush(colors.bgLayer);
    p.drawRoundedRect(contentRect, r, r);

    // 5. 绘制菜单项
    // itemInset: 高亮背景与分割线距底板边缘的统一水平缩进（4px，与底板圆角视觉对齐）
    // textPadding: 文字距底板边缘的水平内边距（12px，ControlHorizontal）
    const int plateLeft    = contentRect.left();
    const int plateWidth   = contentRect.width();
    const int itemInset    = spacing.gap.tight;          // 4
    const int textPadding  = spacing.padding.controlH;   // 12
    const int checkColumn  = hasCheckableAction(this) ? spacing.controlHeight.small : 0; // 24 only when needed
    const QFontMetrics fontMetrics(font());
    const int shortcutColumn = maxShortcutWidth(this, fontMetrics);
    const int trailingColumn = hasSubmenuAction(this) ? ::Spacing::ControlHeight::Small : ::Spacing::Small;

    // 明确设置绘制字体，防止 shadow 循环中字体被污染
    p.setFont(font());

    for (QAction* action : actions()) {
        if (!action->isVisible()) continue;

        QRect itemRect = actionGeometry(action);
        // 规范化水平范围：统一对齐到底板边界（actionGeometry 可能不含 shadow margin）
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

        QColor bg = Qt::transparent;
        if (isEnabled) {
            if (action->isChecked() || isActive)
                bg = colors.subtleSecondary;
        }

        if (bg != Qt::transparent) {
            QRectF bgRect = itemRect.adjusted(itemInset, 1, -itemInset, -1);
            p.setPen(Qt::NoPen);
            p.setBrush(bg);
            p.drawRoundedRect(bgRect, radius.control, radius.control);
        }

        const QColor primaryText = isEnabled ? colors.textPrimary : colors.textDisabled;
        const QColor secondaryText = isEnabled ? colors.textSecondary : colors.textDisabled;

        if (action->isCheckable() && action->isChecked()) {
            QRect checkRect(itemRect.left() + itemInset,
                            itemRect.top() + (itemRect.height() - checkColumn) / 2,
                            checkColumn,
                            checkColumn);
            QFont iconFont(QStringLiteral("Segoe Fluent Icons"));
            iconFont.setPixelSize(12);
            p.setFont(iconFont);
            p.setPen(primaryText);
            p.drawText(checkRect, Qt::AlignCenter, Typography::Icons::CheckMark);
            p.setFont(font());
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
            QFont arrowFont(QStringLiteral("Segoe Fluent Icons"), -1);
            arrowFont.setPixelSize(12);
            p.setFont(arrowFont);
            p.setPen(secondaryText);
            p.drawText(arrowRect, Qt::AlignCenter, Typography::Icons::ChevronRightMed);
            p.setFont(font());
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

    const QSize preferredSize = sizeHint();
    const QSize targetSize = size().expandedTo(preferredSize);
    if (size() != targetSize) {
        resize(targetSize);
        updateGeometry();
    }

    // 修正阴影 margin 偏移。顶层菜单需要完整抵消阴影；级联子菜单保留一个小间距，
    // 避免父菜单阴影区域压到子菜单内容。
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

    // 初始状态：完全透明，揭示进度归零
    m_revealProgress = 0.0;
    setWindowOpacity(0.0);

    // 高度揭示动画（从顶部向下展开，匹配 WinUI 3 PopupThemeTransition）
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

    // 透明度淡入动画
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
    const auto& s = themeShadow(Elevation::High);
    const int layers = 10;
    const int spreadStep = 1;
    int r = themeRadius().overlay;

    for (int i = 0; i < layers; ++i) {
        double ratio = 1.0 - static_cast<double>(i) / layers;
        QColor sc = s.color;
        sc.setAlphaF(s.opacity * ratio * 0.18);

        painter.setPen(Qt::NoPen);
        painter.setBrush(sc);

        int spread = i * spreadStep;
        int offsetY = 2;
        painter.drawRoundedRect(
            contentRect.adjusted(-spread, -spread, spread, spread).translated(0, offsetY),
            r + spread, r + spread);
    }
}

} // namespace view::menus_toolbars
